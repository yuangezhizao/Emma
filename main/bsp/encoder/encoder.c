/**
 * @file encoder.c
 * @brief 旋转编码器 (SIQ-02FVS3) 驱动，使用硬件 PCNT 正交解码
 *
 * 使用 ESP32-S3 的 PCNT (Pulse Counter) 外设进行硬件级正交解码：
 * - A 通道 (GPIO18) 作为边沿信号（检测下降沿）
 * - B 通道 (GPIO3) 作为电平信号（判断方向）
 * - 硬件在 A 下降沿瞬间自动读取 B 电平：
 *   - B=HIGH → 计数 +1 (CW)
 *   - B=LOW  → 计数 -1 (CCW)
 * - A 上升沿不计数（HOLD），与参考项目行为一致
 * - 硬件毛刺滤波器过滤机械接触抖动
 *
 * 为什么从 5ms 软件轮询切换到 PCNT：
 * 诊断数据表明 B 通道 (GPIO3) 在 5ms 轮询中始终为 HIGH，
 * 导致方向判断始终为 CW (+1)。可能原因：
 * 1. B 通道脉冲持续时间极短（< 5ms），软件轮询无法捕捉
 * 2. PCNT 在硬件层面以系统时钟速度（~240MHz）采样 B 电平，
 *    即使 B 脉冲仅持续纳秒级也能正确检测
 *
 * GPIO3 特殊处理：
 * GPIO3 是 ESP32-S3 的 JTAG_SEL 引脚（引导时选择 JTAG 路由）。
 * 为确保 GPIO3 完全脱离引导时的特殊功能，初始化时：
 * 1. 调用 esp_rom_gpio_pad_select_gpio() 在 ROM 层面强制设置
 *    IOMUX 为 GPIO 功能（比 gpio_reset_pin() 更底层）
 * 2. 显式配置为输入模式并启用上拉
 * 3. 在 B 通道注册 GPIO ANYEDGE 中断，统计电平变化次数
 *    作为诊断依据——若中断计数为 0 则确认为硬件问题
 *
 * 使用 GPIO18 (A) 和 GPIO3 (B)。
 */

#include "encoder.h"
#include "bsp/board_def.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"

static const char* TAG = "hal_encoder";

/* ── PCNT 配置参数 ────────────────────────────────────────────── */
#define ENCODER_PCNT_HIGH_LIMIT 32767
#define ENCODER_PCNT_LOW_LIMIT  (-32768)
#define ENCODER_GLITCH_NS       10000 /* 10us 毛刺滤波，过滤机械抖动 */

/* ── PCNT 句柄 ────────────────────────────────────────────────── */
static pcnt_unit_handle_t s_pcnt_unit = NULL;
static pcnt_channel_handle_t s_pcnt_chan_a = NULL;

/* ── 诊断：B 通道 GPIO 中断计数器 ─────────────────────────────── */
/* 统计 B 通道 (GPIO3) 发生的所有电平变化次数（ANYEDGE）。
 * 若旋转编码器后此值仍为 0，则 B 通道硬件确实无信号。 */
static volatile uint32_t s_b_isr_count = 0;
static bool s_b_diag_installed = false;

/**
 * @brief B 通道 GPIO 中断处理函数（IRAM 中执行）
 *
 * 统计 B 通道 (GPIO3) 的电平变化次数。
 * 用于验证 B 通道硬件连接是否正常。
 * 即使脉冲只有微秒级，中断也能捕捉到。
 */
static void IRAM_ATTR b_channel_isr_handler(void* arg)
{
    s_b_isr_count++;
}

/* ── 模块状态 ────────────────────────────────────────────────── */
static bool s_initialized = false;

esp_err_t hal_encoder_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_err_t ret;

    /* ══════════════════════════════════════════════════════════════
     *  GPIO3 特殊初始化：ROM 层面强制切换到 GPIO 功能
     *
     *  GPIO3 是 JTAG_SEL 引脚，引导时可能被设置为特殊功能。
     *  esp_rom_gpio_pad_select_gpio() 直接写 IOMUX 寄存器，
     *  比 gpio_reset_pin() 更底层，确保完全断开引导时的路由。
     *
     *  同时对 GPIO18 也做同样处理以保持一致性。
     * ══════════════════════════════════════════════════════════════ */
    esp_rom_gpio_pad_select_gpio(PIN_ENCODER_A);
    esp_rom_gpio_pad_select_gpio(PIN_ENCODER_B);
    gpio_reset_pin(PIN_ENCODER_A);
    gpio_reset_pin(PIN_ENCODER_B);

    /* ── 显式配置 A/B 为输入 + 上拉（在 PCNT 接管前确认） ──── */
    gpio_config_t ab_cfg = {
        .pin_bit_mask = (1ULL << PIN_ENCODER_A) | (1ULL << PIN_ENCODER_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&ab_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoder A/B GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 验证 GPIO3 初始状态 ──────────────────────────────────── */
    int init_a = gpio_get_level(PIN_ENCODER_A);
    int init_b = gpio_get_level(PIN_ENCODER_B);
    ESP_LOGI(TAG, "GPIO pre-PCNT levels: A(GPIO%d)=%d, B(GPIO%d)=%d", PIN_ENCODER_A, init_a, PIN_ENCODER_B, init_b);

    /* ── 配置编码器按压按钮（低电平有效，内部上拉） ──────────── */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_ENCODER_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&btn_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoder button GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ══════════════════════════════════════════════════════════════
     *  PCNT 硬件正交解码设置
     *
     *  PCNT 通过 GPIO 矩阵直接采样 A/B 引脚信号。
     *  当 A 通道产生下降沿时，PCNT 硬件以系统时钟速度
     *  （~240MHz）瞬间读取 B 通道电平，判断旋转方向。
     *  这比任何软件轮询方案都要快且准确。
     * ══════════════════════════════════════════════════════════════ */

    /* ── 创建 PCNT 单元 ──────────────────────────────────────── */
    pcnt_unit_config_t unit_config = {
        .high_limit = ENCODER_PCNT_HIGH_LIMIT,
        .low_limit = ENCODER_PCNT_LOW_LIMIT,
    };
    ret = pcnt_new_unit(&unit_config, &s_pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT unit create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 配置毛刺滤波器 ──────────────────────────────────────── */
    /* 过滤 <= 10us 的短脉冲（机械触点抖动）。
     * PCNT 硬件滤波器最大约 13us（受 APB 时钟限制）。 */
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = ENCODER_GLITCH_NS,
    };
    ret = pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PCNT glitch filter config failed: %s (continuing without filter)", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    /* ── 创建 PCNT 通道：A 为边沿信号，B 为电平信号 ──────────── */
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = PIN_ENCODER_A,
        .level_gpio_num = PIN_ENCODER_B,
    };
    ret = pcnt_new_channel(s_pcnt_unit, &chan_a_config, &s_pcnt_chan_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT channel create failed: %s", esp_err_to_name(ret));
        pcnt_del_unit(s_pcnt_unit);
        s_pcnt_unit = NULL;
        return ret;
    }

    /* ── 配置计数行为 ────────────────────────────────────────── */
    /* A 上升沿 → HOLD（不计数）
     * A 下降沿 → INCREASE（计数）
     * 与参考项目 Encoder.cpp 行为一致：只在 A 下降沿做一次方向判断 */
    ret = pcnt_channel_set_edge_action(s_pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_HOLD, /* A 上升沿：不计数 */
                                       PCNT_CHANNEL_EDGE_ACTION_INCREASE);           /* A 下降沿：计数 */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT edge action config failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* B=HIGH → KEEP（正常方向，INCREASE = +1 = CW）
     * B=LOW  → INVERSE（反转方向，INCREASE 变为 DECREASE = -1 = CCW） */
    ret = pcnt_channel_set_level_action(s_pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, /* B=HIGH → CW (+1) */
                                        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);            /* B=LOW → CCW (-1) */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT level action config failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* ── 确保 A/B 引脚上拉在 PCNT 配置后仍然生效 ──────────── */
    /* PCNT 驱动可能在 pcnt_new_channel() 中重新配置 GPIO，
     * 这里再次设置上拉以确保正确 */
    gpio_set_pull_mode(PIN_ENCODER_A, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_ENCODER_B, GPIO_PULLUP_ONLY);

    /* ── 清零并启动 PCNT ─────────────────────────────────────── */
    ret = pcnt_unit_enable(s_pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT unit enable failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ret = pcnt_unit_clear_count(s_pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT unit clear count failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ret = pcnt_unit_start(s_pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCNT unit start failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* ══════════════════════════════════════════════════════════════
     *  B 通道 GPIO 中断诊断
     *
     *  在 B 通道 (GPIO3) 上注册 ANYEDGE 中断，
     *  统计电平变化次数。即使脉冲极短（微秒级），
     *  GPIO 中断也能捕捉。
     *  这与 PCNT 通过不同信号路径工作，互不干扰：
     *  - PCNT：通过 GPIO 矩阵读取信号
     *  - GPIO ISR：通过 GPIO 中断矩阵触发
     * ══════════════════════════════════════════════════════════════ */
    s_b_isr_count = 0;
    ret = gpio_set_intr_type(PIN_ENCODER_B, GPIO_INTR_ANYEDGE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "B-channel ISR intr type config failed: %s (diagnostic only)", esp_err_to_name(ret));
    }
    /* gpio_install_isr_service 若已安装会返回 ESP_ERR_INVALID_STATE，可忽略 */
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR service install failed: %s (diagnostic only)", esp_err_to_name(isr_ret));
    }
    ret = gpio_isr_handler_add(PIN_ENCODER_B, b_channel_isr_handler, NULL);
    if (ret == ESP_OK) {
        s_b_diag_installed = true;
    } else {
        ESP_LOGW(TAG, "B-channel ISR handler add failed: %s (diagnostic only)", esp_err_to_name(ret));
    }

    s_initialized = true;

    int a = gpio_get_level(PIN_ENCODER_A);
    int b = gpio_get_level(PIN_ENCODER_B);
    ESP_LOGI(TAG, "Encoder initialized (PCNT hardware decoder): A=GPIO%d, B=GPIO%d, BTN=GPIO%d", PIN_ENCODER_A,
             PIN_ENCODER_B, PIN_ENCODER_BTN);
    ESP_LOGI(TAG, "Initial levels: A=%d, B=%d", a, b);
    ESP_LOGI(TAG, "PCNT config: A falling edge -> count, B level -> direction (KEEP/INVERSE)");
    ESP_LOGI(TAG, "Glitch filter: %d ns, B-channel ISR: %s", ENCODER_GLITCH_NS,
             s_b_diag_installed ? "ENABLED" : "DISABLED");

    return ESP_OK;

cleanup:
    if (s_pcnt_chan_a) {
        pcnt_del_channel(s_pcnt_chan_a);
        s_pcnt_chan_a = NULL;
    }
    if (s_pcnt_unit) {
        pcnt_del_unit(s_pcnt_unit);
        s_pcnt_unit = NULL;
    }
    return ret;
}

esp_err_t hal_encoder_poll(void)
{
    /*
     * PCNT 模式下轮询为空操作。
     * 计数由 PCNT 硬件自动更新，保留此接口以兼容测试代码调用。
     */
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t hal_encoder_get_count(int* count)
{
    if (!s_initialized || count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return pcnt_unit_get_count(s_pcnt_unit, count);
}

esp_err_t hal_encoder_clear_count(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return pcnt_unit_clear_count(s_pcnt_unit);
}

esp_err_t hal_encoder_get_raw_levels(int* level_a, int* level_b)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (level_a) {
        *level_a = gpio_get_level(PIN_ENCODER_A);
    }
    if (level_b) {
        *level_b = gpio_get_level(PIN_ENCODER_B);
    }
    return ESP_OK;
}

bool hal_encoder_button_pressed(void)
{
    if (!s_initialized) {
        return false;
    }
    return gpio_get_level(PIN_ENCODER_BTN) == 0; /* 低电平有效 */
}

uint32_t hal_encoder_get_b_isr_count(void)
{
    return s_b_isr_count;
}

esp_err_t hal_encoder_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* ── 移除 B 通道中断 ─────────────────────────────────────── */
    if (s_b_diag_installed) {
        gpio_isr_handler_remove(PIN_ENCODER_B);
        s_b_diag_installed = false;
    }

    /* ── 停止并释放 PCNT ─────────────────────────────────────── */
    if (s_pcnt_unit) {
        pcnt_unit_stop(s_pcnt_unit);
        pcnt_unit_disable(s_pcnt_unit);
    }
    if (s_pcnt_chan_a) {
        pcnt_del_channel(s_pcnt_chan_a);
        s_pcnt_chan_a = NULL;
    }
    if (s_pcnt_unit) {
        pcnt_del_unit(s_pcnt_unit);
        s_pcnt_unit = NULL;
    }

    gpio_reset_pin(PIN_ENCODER_A);
    gpio_reset_pin(PIN_ENCODER_B);
    gpio_reset_pin(PIN_ENCODER_BTN);

    s_initialized = false;

    ESP_LOGI(TAG, "Encoder deinitialized");
    return ESP_OK;
}
