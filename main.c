#include <stdio.h>
#include "esp32_s3_szp.h"
#include "logo_en_240x240_lcd.h"
#include "yingwu.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "app_ui.h"
#define BSP_SD_CLK (47)
#define BSP_SD_CMD (48)
#define BSP_SD_D0 (21)
static const char *TAG = "main";
#define MOUNT_POINT "/sdcard"
extern int to_photo_flag;

static QueueHandle_t gpio_evt_queue = NULL; // 定义队列句柄
// 主界面 任务函数
// GPIO中断服务函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;                  // 获取入口参数
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); // 把入口参数值发送到队列
    
}

// GPIO任务函数
static void gpio_task_example(void *arg)
{
    uint32_t io_num; // 定义变量 表示哪个GPIO
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {                                                                                // 死等队列消息
            printf("GPIO[%" PRIu32 "] intr, val: %d\n", io_num, gpio_get_level(io_num)); // 打印相关内容
            to_photo_flag = 1;
        }
    }
}


/***************************  boot按键 ↑  *******************************************/
/*******************************************************************************/

void app_main(void)
{
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,   // 如果挂载不成功是否需要格式化SD卡
        .max_files = 5,                   // 允许打开的最大文件数
        .allocation_unit_size = 32 * 1024 // 分配单元大小
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();                      // SDMMC主机接口配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // SDMMC插槽配置
    slot_config.width = 1;                                         // 设置为1线SD模式
    slot_config.clk = BSP_SD_CLK;
    slot_config.cmd = BSP_SD_CMD;
    slot_config.d0 = BSP_SD_D0;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 打开内部上拉电阻

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card); // 挂载SD卡

    if (ret != ESP_OK)
    { // 如果没有挂载成功
        if (ret == ESP_FAIL)
        { // 如果挂载失败
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
        }
        else
        { // 如果是其它错误 打印错误名称
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted"); // 提示挂载成功
    sdmmc_card_print_info(stdout, card); // 终端打印SD卡的一些信息



/***************************  sd ↑  *******************************************/
/*******************************************************************************/

    gpio_config_t io0_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,  // 下降沿中断
        .mode = GPIO_MODE_INPUT,         // 输入模式
        .pin_bit_mask = 1 << GPIO_NUM_0, // 选择GPIO0
        .pull_down_en = 0,               // 禁能内部下拉
        .pull_up_en = 1                  // 使能内部上拉
    };
    // 根据上面的配置 设置GPIO
    gpio_config(&io0_conf);

    // 创建一个队列初始GPIO事件
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    // 开启GPIO任务
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
    // 创建GPIO中断服务
    gpio_install_isr_service(0);
    // 给GPIO0添加中断处理
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void *)GPIO_NUM_0);
    /***************************  boot按键 ↑  *******************************************/
    /*******************************************************************************/

    bsp_i2c_init();  // I2C初始化
    pca9557_init();  // IO扩展芯片初始化
    bsp_lcd_init();  // 液晶屏初始化
    bsp_lvgl_start();
    vTaskDelay(500 / portTICK_PERIOD_MS); // 延时500毫秒
    bsp_camera_init(); // 摄像头初始化
    app_camera_lcd(); // 让摄像头画面显示到LCD上
   
}
