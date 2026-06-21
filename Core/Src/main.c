/* STM32F103C8 任务一完整控制程序 - 兼容 PB3 蜂鸣器 */
#include "main.h"

/* --- 硬件引脚定义 --- */
#define KEY_1_PIN        GPIO_PIN_5
#define KEY_1_PORT       GPIOA
#define KEY_2_PIN        GPIO_PIN_2
#define KEY_2_PORT       GPIOA

#define LED_RED_PIN      GPIO_PIN_6
#define LED_RED_PORT     GPIOB
#define LED_GREEN_PIN    GPIO_PIN_5
#define LED_GREEN_PORT   GPIOB
#define LED_BLUE_PIN     GPIO_PIN_4
#define LED_BLUE_PORT    GPIOB

/* --- 蜂鸣器引脚 (根据你的实际硬件修改为 PB3) --- */
#define BUZZER_PIN       GPIO_PIN_3
#define BUZZER_PORT      GPIOB

/* --- 状态枚举 --- */
typedef enum {
    RGB_GREEN_SOLID = 0,
    RGB_RED_SOLID,
    RGB_GREEN_BREATHING
} RGB_Mode_t;

typedef enum {
    BUZZER_CONTINUOUS = 0,
    BUZZER_SILENT,
    BUZZER_INTERMITTENT
} Buzzer_Mode_t;

/* --- 全局变量 --- */
static RGB_Mode_t    Current_RGB_Mode = RGB_GREEN_SOLID;
static Buzzer_Mode_t Current_Buzzer_Mode = BUZZER_CONTINUOUS;
static uint8_t       Key2_LongPress_Active = 0;

/* --- 函数声明 --- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);

static uint8_t Get_Key_State(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
static void Process_Key1_ShortPress(void);
static void Process_Key2_LongPress(void);
static void Update_RGB_LED(void);
static void Handle_Breathing_LED(void);
static void Update_Buzzer(void);

/* --- 主函数 --- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* 初始化蜂鸣器：低电平触发，初始状态为持续鸣响 */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    while (1)
    {
        Process_Key1_ShortPress(); // 处理按键1 (短按)
        Process_Key2_LongPress();  // 处理按键2 (长按)
        Update_RGB_LED();          // 更新RGB状态
        Update_Buzzer();           // 更新蜂鸣器状态
        HAL_Delay(10);
    }
}

/* --------------------------------------------------------------
 * 函数名称：Get_Key_State
 * 功能描述：读取按键状态（带10ms软件消抖）
 * 输入参数：
 *     GPIOx  ：GPIO端口基地址，例如 GPIOA, GPIOB
 *     GPIO_Pin：具体的引脚编号，例如 GPIO_PIN_2, GPIO_PIN_5
 * 返回值：
 *     1 ：按键处于按下状态（已通过消抖验证）
 *     0 ：按键处于未按下状态
 * -------------------------------------------------------------- */
static uint8_t Get_Key_State(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
    /* 1. 第一次检测：读取当前引脚电平 */
    if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET)  // 如果读到低电平（假设按键按下为低电平）
    {
        /* 2. 软件消抖：延时10ms，等待按键电平稳定 */
        HAL_Delay(10);   // 这里利用HAL库的延时函数，避免按键弹跳导致的误判
        
        /* 3. 第二次检测：再次读取引脚电平进行确认 */
        if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET)  // 如果延时后仍然是低电平
        {
            return 1;    // 确认按键被按下，返回状态 1
        }
    }
    
    /* 4. 如果第一次检测不是低电平，或者消抖后电平变为高电平，说明按键未按下 */
    return 0;            // 返回状态 0
}

/* ------------------------------------------------------------------
 * 函数名称：Process_Key1_ShortPress
 * 功能描述：检测 KEY1 的短按事件，并同步切换 RGB 与蜂鸣器的状态。
 * 输入参数：无（直接读取全局配置宏 KEY_1_PORT, KEY_1_PIN）
 * 返回值：无
 * ------------------------------------------------------------------ */
static void Process_Key1_ShortPress(void)
{
    /* 1. 定义静态变量（记录状态，函数退出后值不丢失） */
    static uint32_t last_time = 0;    // 记录按键按下的起始时间戳
    static uint8_t is_pressed = 0;    // 记录按键是否处于按下状态 (1=是, 0=否)

    /* 2. 调用消抖函数检测按键是否被按下 */
    if (Get_Key_State(KEY_1_PORT, KEY_1_PIN) == 1) // 检测到按键按下
    {
        /* 如果是刚刚按下（之前未按下） */
        if (!is_pressed) 
        { 
            is_pressed = 1;           // 标记按键为“已按下”
            last_time = HAL_GetTick(); // 记录当前系统滴答时钟（毫秒级）
        }
    }
    else // 未检测到按键按下（按键已松开）
    {
        /* 如果之前的状态是“已按下”，说明现在才刚刚松开 */
        if (is_pressed)
        {
            is_pressed = 0;           // 重置按下标记
            
            /* 3. 计算按键按下的持续时间 */
            uint32_t press_duration = HAL_GetTick() - last_time;
            
            /* 4. 判定是否为“短按”：持续时间必须在 20ms 到 800ms 之间 */
            /*    20ms以下视为抖动的杂波，800ms以上视为长按 */
            if (press_duration > 20 && press_duration < 800) 
            {
                /* ---------- 任务一：切换 RGB 灯状态 ---------- */
                // 采用状态机轮转模式：绿常亮 -> 红常亮 -> 绿呼吸 -> 回到绿常亮
                if (Current_RGB_Mode == RGB_GREEN_SOLID) 
                    Current_RGB_Mode = RGB_RED_SOLID;
                else if (Current_RGB_Mode == RGB_RED_SOLID) 
                    Current_RGB_Mode = RGB_GREEN_BREATHING;
                else if (Current_RGB_Mode == RGB_GREEN_BREATHING) 
                    Current_RGB_Mode = RGB_GREEN_SOLID;

                /* ---------- 任务二：切换蜂鸣器状态 ---------- */
                // 同步轮转：长鸣 -> 静默 -> 断续 -> 回到长鸣
                if (Current_Buzzer_Mode == BUZZER_CONTINUOUS) 
                    Current_Buzzer_Mode = BUZZER_SILENT;
                else if (Current_Buzzer_Mode == BUZZER_SILENT) 
                    Current_Buzzer_Mode = BUZZER_INTERMITTENT;
                else if (Current_Buzzer_Mode == BUZZER_INTERMITTENT) 
                    Current_Buzzer_Mode = BUZZER_CONTINUOUS;
            }
        }
    }
}

/* ------------------------------------------------------------------
 * 函数名称：Process_Key2_LongPress
 * 功能描述：检测 KEY2 的长按事件（持续按下超过 1000ms 判定为长按）
 *           并在长按时激活 LongPress 标志，松手时清空该标志。
 * 输入参数：无（直接读取全局配置宏 KEY_2_PORT, KEY_2_PIN）
 * 返回值：无
 * ------------------------------------------------------------------ */
static void Process_Key2_LongPress(void)
{
    /* 1. 定义静态变量（记录按键状态和时间） */
    static uint32_t press_start_time = 0; // 记录按键按下的起始时间戳
    static uint8_t key2_is_pressed = 0;   // 记录按键当前是否处于按下状态 (1=是, 0=否)

    /* 2. 分支一：【检测到按键刚按下】逻辑 */
    /* 条件：读到低电平（按下） 且 之前未标记为按下 */
    if (HAL_GPIO_ReadPin(KEY_2_PORT, KEY_2_PIN) == GPIO_PIN_RESET && !key2_is_pressed)
    {
        HAL_Delay(20); // 消抖延时 20ms
        if (HAL_GPIO_ReadPin(KEY_2_PORT, KEY_2_PIN) == GPIO_PIN_RESET) // 再次确认按下
        {
            key2_is_pressed = 1;        // 标记按键状态为“已按下”
            press_start_time = HAL_GetTick(); // 记录按下瞬间的毫秒级时间戳
        }
    }
    /* 3. 分支二：【检测到按键松开】逻辑 */
    /* 条件：读到高电平（松开） 且 之前标记为按下 */
    else if (HAL_GPIO_ReadPin(KEY_2_PORT, KEY_2_PIN) == GPIO_PIN_SET && key2_is_pressed)
    {
        key2_is_pressed = 0;            // 重置按键状态
        Key2_LongPress_Active = 0;      // 【关键】松手立刻清除长按激活标志！
    }
    /* 4. 分支三：【检测到按键长按持续中】逻辑 */
    /* 条件：读到低电平（按着没放） 且 之前已标记为按下 */
    else if (HAL_GPIO_ReadPin(KEY_2_PORT, KEY_2_PIN) == GPIO_PIN_RESET && key2_is_pressed)
    {
        /* 计算当前时间与按下时间的差值，如果持续按住 >= 1000ms (1秒) */
        if ((HAL_GetTick() - press_start_time) >= 1000) 
        {
            Key2_LongPress_Active = 1;  // 激活长按标志位！
        }
    }
}

/* ------------------------------------------------------------------
 * 函数名称：Update_RGB_LED
 * 功能描述：根据当前的逻辑状态更新 RGB 灯的亮灭情况。
 *           包含优先级判断：KEY2 长按状态的优先级最高。
 * 输入参数：无
 * 返回值：无
 * ------------------------------------------------------------------ */
static void Update_RGB_LED(void)
{
    /* 1. 步骤一：【先全部关闭】*/
    /* 为防止混色或残留亮光，先把所有颜色的灯强制熄灭 */
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,   LED_RED_PIN,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT,  LED_BLUE_PIN,  GPIO_PIN_RESET);

    /* 2. 步骤二：【优先级最高的判断】 判断是否处于 KEY2 长按激活状态 */
    if (Key2_LongPress_Active == 1) {
        /* 如果 KEY2 长按激活，无论之前是什么模式，强制只亮绿灯！ */
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        return; /* 关键点！立刻退出函数，不再执行后面的普通模式逻辑 */
    }

    /* 3. 步骤三：【普通模式逻辑】 如果没有触发长按，才根据 KEY1 设定的模式来亮灯 */
    if (Current_RGB_Mode == RGB_GREEN_SOLID) {
        /* 模式1：绿灯常亮 */
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    }
    else if (Current_RGB_Mode == RGB_RED_SOLID) {
        /* 模式2：红灯常亮 */
        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
    }
    else if (Current_RGB_Mode == RGB_GREEN_BREATHING) {
        /* 模式3：绿灯呼吸（调用专门的呼吸灯处理函数） */
        Handle_Breathing_LED();
    }
}

/* ------------------------------------------------------------------
 * 函数名称：Handle_Breathing_LED
 * 功能描述：使用软件模拟 PWM 的方式，让绿灯实现“呼吸”效果。
 *           呼吸周期：2秒 (1秒渐亮 + 1秒渐暗)。
 *           刷新频率：每10ms更新一次 IO 口状态。
 * 输入参数：无
 * 返回值：无
 * ------------------------------------------------------------------ */
static void Handle_Breathing_LED(void)
{
    /* 1. 静态变量：记录上一次更新 IO 的时间 */
    static uint32_t last_time = 0; 
    
    /* 2. 获取当前系统运行时间（毫秒级） */
    uint32_t current_tick = HAL_GetTick(); 
    
    /* 3. 计算当前时间在 2000ms (2秒) 周期内的位置 */
    /*    假设 0ms 是周期的开始，2000ms 是周期的结束 */
    uint32_t cycle_time = current_tick % 2000; 
    
    /* 4. 定义占空比（亮度百分比），范围 0~100 */
    uint32_t duty_cycle = 0; 

    /* 5. 亮度曲线计算：根据在周期内的位置，计算出当前的亮度比例 */
    if (cycle_time < 1000) {
        /* 第一阶段：0ms 到 1000ms (前半段) -> 亮度从 0% 逐渐升到 100% */
        duty_cycle = (cycle_time * 100) / 1000; 
    } else {
        /* 第二阶段：1000ms 到 2000ms (后半段) -> 亮度从 100% 逐渐降到 0% */
        duty_cycle = ((2000 - cycle_time) * 100) / 1000; 
    }

    /* 6. 定时刷新 IO 口：每 10ms 更新一次，避免刷新过快导致 CPU 卡顿 */
    if ((current_tick - last_time) >= 10) {
        last_time = current_tick; // 更新上次刷新时间

        /* 7. 根据亮度比例控制 LED 亮灭 */
        /*    核心逻辑：用一个阈值（50%）来模拟 PWM 的占空比。
           当亮度 > 50% 时，LED 亮；当亮度 <= 50% 时，LED 灭。
           由于刷新速度极快 (10ms)，人眼会看到亮度随 duty_cycle 变化。 */
        if (duty_cycle > 50) {
            /* 亮度高时点亮 */
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        } else {
            /* 亮度低时熄灭 */
            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
        }
    }
}

/* ------------------------------------------------------------------
 * 函数名称：Update_Buzzer
 * 功能描述：根据当前蜂鸣器模式（Current_Buzzer_Mode）控制蜂鸣器引脚。
 *           低电平触发：低电平(RESET) = 响，高电平(SET) = 静默。
 * 输入参数：无
 * 返回值：无
 * ------------------------------------------------------------------ */
static void Update_Buzzer(void)
{
    /* 1. 定义静态变量：记录上一次改变蜂鸣器状态的时间戳 (毫秒)。
       使用 static 关键字，确保这个变量在函数退出后依然保留其值。*/
    static uint32_t last_toggle_time = 0;

    /* 2. 模式1：【连续鸣响】 */
    if (Current_Buzzer_Mode == BUZZER_CONTINUOUS) {
        /* 
           低电平触发 => 输出低电平(RESET) 
           蜂鸣器会持续保持在这个状态，一直发声。
        */
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); 
    }
    /* 3. 模式2：【静默】 */
    else if (Current_Buzzer_Mode == BUZZER_SILENT) {
        /* 
           低电平触发 => 输出高电平(SET)
           蜂鸣器会持续保持在这个状态，一直静音。
        */
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   
    }
    /* 4. 模式3：【间断鸣响】（每500ms翻转一次状态） */
    else if (Current_Buzzer_Mode == BUZZER_INTERMITTENT) {
        /* 检查距离上一次翻转是否已经过去了 500ms */
        if ((HAL_GetTick() - last_toggle_time) >= 500) {
            /* 更新时间戳为当前时间 */
            last_toggle_time = HAL_GetTick();
            
            /* 
               翻转引脚状态：如果当前是SET(高电平/静音)，就变成RESET(低电平/响)；
               反之则变成SET。由此产生“滴...滴...滴...”的间断效果。
            */
            HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
        }
    }
}

/* --- 系统初始化--- */
void SystemClock_Config(void) { }

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 按键 PA5, PA2 上拉输入 */
  GPIO_InitStruct.Pin = KEY_2_PIN | KEY_1_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* LED PB4, PB5, PB6 推挽输出 */
  GPIO_InitStruct.Pin = LED_BLUE_PIN | LED_GREEN_PIN | LED_RED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 蜂鸣器 PB3 推挽输出 */
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}