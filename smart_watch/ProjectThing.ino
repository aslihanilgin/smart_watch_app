/**
* Author: Aslihan Ilgin Okan
* Date: May 2023
* 
* Program that displays a watch screen and has two additional functionalities
* of plant watering reminder and displaying meme pictures at specific times.
* 
* References: TTGO T-Watch Library Examples
*/

#include "config.h"
#include <string>     // std::string, std::to_string
#include <Preferences.h>

Preferences preferences; // Declare a global variable to hold the preferences object

// timing 
typedef struct {
    lv_obj_t *hour;
    lv_obj_t *minute;
    lv_obj_t *second;
} str_datetime_t;

static str_datetime_t g_data;
TTGOClass *watch = nullptr;
PCF8563_Class *rtc;

bool irq = false;

LV_FONT_DECLARE(morgnite_bold_64); // font
LV_IMG_DECLARE(background_wallpaper); // watch background photo

// #define DEFAULT_SCREEN_TIMEOUT  60*1000 // uncomment if you need timeout

lv_obj_t *go_back_button, *main_screen, *meme_screen, *bg_image, *watering_alert_box;

const char * btn_str[] = {"Close", ""};

RTC_Date curr_datetime;
int rdm_hour = 10; // placeholder values for random 
int rdm_min = 20; // time values, can change these
int rdm_sec = 1;  // to help with testing

struct RandomTimeVals {
  int hour;
  int min;
  int sec;
};

// Save the variable values to flash memory
void saveVariables() {
  preferences.begin("myApp");
  preferences.putInt("rdm_hour", rdm_hour);
  preferences.putInt("rdm_min", rdm_min);
  preferences.putInt("rdm_sec", rdm_sec);
  preferences.end();
}

// Restore the variable values from flash memory
void restoreVariables() {
  preferences.begin("myApp");
  rdm_hour = preferences.getInt("rdm_hour", 0);
  rdm_min = preferences.getInt("rdm_min", 0);
  rdm_sec = preferences.getInt("rdm_sec", 0);
  preferences.end();
}

void setup()
{
    Serial.begin(115200);
    watch = TTGOClass::getWatch();
    watch->begin(); 
    watch->lvgl_begin();
    rtc = watch->rtc;
    
    // Use compile time
    rtc->check();

    watch->openBL();

    //Lower the brightness
    watch->bl->adjust(100);

    // Initialize the preferences object
    preferences.begin("myApp");

    // button on the watch setup
    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(AXP202_INT, [] {
      irq = true;
    }, FALLING);
    //!Clear IRQ unprocessed  first
    watch->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    watch->power->clearIRQ();

    // add the background image to the background screen
    bg_image = lv_img_create(lv_scr_act(), NULL);
    lv_img_set_src(bg_image, &background_wallpaper);
    lv_obj_align(bg_image, NULL, LV_ALIGN_CENTER, 0, 0);

    // font
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&style, LV_STATE_DEFAULT, &morgnite_bold_64);

    // time 
    g_data.hour = lv_label_create(bg_image, nullptr);
    lv_obj_add_style(g_data.hour, LV_OBJ_PART_MAIN, &style);
    lv_label_set_text(g_data.hour, "00");
    lv_obj_align(g_data.hour, bg_image, LV_ALIGN_IN_TOP_MID, 10, 30);

    g_data.minute = lv_label_create(bg_image, nullptr);
    lv_obj_add_style(g_data.minute, LV_OBJ_PART_MAIN, &style);
    lv_label_set_text(g_data.minute, "00");
    lv_obj_align(g_data.minute, g_data.hour, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    g_data.second = lv_label_create(bg_image, nullptr);
    lv_obj_add_style(g_data.second, LV_OBJ_PART_MAIN, &style);
    lv_label_set_text(g_data.second, "00");
    lv_obj_align(g_data.second, g_data.minute, LV_ALIGN_OUT_RIGHT_MID, 9, 0);

    lv_task_create([](lv_task_t *t) {

        curr_datetime = rtc->getDateTime();
        lv_label_set_text_fmt(g_data.second, "%02u", curr_datetime.second);
        lv_label_set_text_fmt(g_data.minute, "%02u", curr_datetime.minute);
        lv_label_set_text_fmt(g_data.hour, "%02u", curr_datetime.hour);

    }, 1000, LV_TASK_PRIO_MID, nullptr);

    // Set 20MHz operating speed to reduce power consumption
    setCpuFrequencyMhz(20);

    // Restore the time variable values from flash memory
    restoreVariables();
}

void loop() {   
  Serial.begin(115200);

  // uncomment if you need timeout
  // if (lv_disp_get_inactive_time(NULL) > DEFAULT_SCREEN_TIMEOUT) {
  //   saveVariables();
  //   watch->bl->off();
  //   watch->displaySleep();
  //   watch->powerOff();

  //   // TOUCH SCREEN  Wakeup source
  //   esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);

  //   // BUTTON Wakeup source
  //   esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);

  //   esp_deep_sleep_start();
  // } 
  
  checkButtonPress(); // check if watch button is pressed

  // change the random time values every day a minute before midnight
  curr_datetime = rtc->getDateTime();
  if (curr_datetime.hour == 23 && 
      curr_datetime.minute == 59 &&
      curr_datetime.second == 59) {
  
        RandomTimeVals timeVals = changeRandomTimeVals(); // create RandomTimeVals obj
        rdm_hour = timeVals.hour;
        rdm_min = timeVals.min;
        rdm_sec = timeVals.sec; 

  }
  
  checkMemeOClock(rdm_hour, rdm_min, rdm_sec); // check if it's meme o'clock
  checkPlantWateringTime(); // check if it's plant watering time

  lv_task_handler();
}

/**
 * @brief  RandomTimeVals class that changes random time values every 
 * day a minute before midnight.
 * @param  void
 * @return RandomTimeVals object
 */
// changes the random time values every day a minute before midnight
RandomTimeVals changeRandomTimeVals() {
  
  RandomTimeVals timeVals;

  curr_datetime = rtc->getDateTime();
  
  // generate random number
  timeVals.hour = 1+ (rand() % 23); // hour
  timeVals.min = 1+ (rand() % 59); // minute
  timeVals.sec = 1+ (rand() % 59); // seconds

  return timeVals;

}

/**
 * @brief  Check if it is planting time and execute reminder functions. Plant watering
 * reminder time is hardcoded to Wednesday, 9pm. Displays a reminder message on the 
 * screen and does not disappear until the "Close" button is pressed.
 * @param  void
 * @return void
 */
void checkPlantWateringTime() {

  int WEEKLY_EVENT_DAY     = 3; // event start day: day number (Wednesday == 3)
  int WEEKLY_EVENT_START_H = 21; // event start time: hour 
  int WEEKLY_EVENT_START_M = 0; // event start time: minute
  int WEEKLY_EVENT_START_S = 1; // event start time: second

  RTC_Date curr_datetime = rtc->getDateTime();

  if (curr_datetime.day          == WEEKLY_EVENT_DAY     &&
      curr_datetime.hour         == WEEKLY_EVENT_START_H &&
      curr_datetime.minute       == WEEKLY_EVENT_START_M &&
      curr_datetime.second       == WEEKLY_EVENT_START_S) {

      // set label for meme pic
      watering_alert_box = lv_msgbox_create(bg_image, nullptr);
      lv_msgbox_set_text(watering_alert_box, "Time to water your plants!");
      lv_msgbox_add_btns(watering_alert_box, btn_str);
      lv_obj_set_width(watering_alert_box, 200);
      lv_obj_set_event_cb(watering_alert_box, event_handler);
      lv_obj_align(watering_alert_box, NULL, LV_ALIGN_CENTER, 0, 0); 
      
      delay(1000); // delay to avoid overloading

  } 

}

/**
 * @brief  Check if it is Meme O'Clock and execute Meme O'Clock functions. Generates
 * random time values each day and shows a random meme picture out of five available.
 * Displays the message "It'S mEmE OClOcK" on the meme picture and removes both from 
 * the screen after 10 seconds. 
 * @param  int hour_value
 * @param  int minute_value
 * @param  int second_value
 * @return void
 */
void checkMemeOClock(int rdm_hour, int rdm_min, int rdm_sec) {

  srand(rand()); // set a random seed

  RTC_Date curr_datetime = rtc->getDateTime();

  if (curr_datetime.hour == rdm_hour && curr_datetime.minute == rdm_min && curr_datetime.second == rdm_sec) {

    int rdm_meme_no = 1+ (rand() % 5); // random number to choose the meme pic

    lv_obj_t *meme_img = lv_img_create(lv_scr_act(), NULL);
    
    std::string meme_file_name = "meme_" + std::to_string(rdm_meme_no);
    delay(1000); // delay to avoid overloading

    switch (rdm_meme_no) {
      case 1:
          LV_IMG_DECLARE(meme_1);
          lv_img_set_src(meme_img, &meme_1);
          break;
      case 2:
          LV_IMG_DECLARE(meme_2);
          lv_img_set_src(meme_img, &meme_2);
          break;
      case 3:
          LV_IMG_DECLARE(meme_3);
          lv_img_set_src(meme_img, &meme_3);
          break;
      case 4:
          LV_IMG_DECLARE(meme_4);
          lv_img_set_src(meme_img, &meme_4);
          break;
      case 5:
          LV_IMG_DECLARE(meme_5);
          lv_img_set_src(meme_img, &meme_5);
          break;
      default:
          LV_IMG_DECLARE(meme_1);
          lv_img_set_src(meme_img, &meme_1);
          break;
    }   
    
    lv_obj_align(meme_img, NULL, LV_ALIGN_CENTER, 0, 0);

    // set label for meme pic
    lv_obj_t * meme_label = lv_label_create(meme_img, nullptr);
    lv_label_set_text(meme_label, "It'S mEmE OClOcK");

    lv_obj_align(g_data.hour, meme_img, LV_ALIGN_IN_TOP_MID, 10, 30);
    delay(1000); // delay to avoid overloading

    lv_task_create(remove_obj_task, 10000, LV_TASK_PRIO_LOWEST, meme_label); // Create a task to remove the label after 10 seconds
    delay(500); // delay to avoid overloading

    lv_task_create(remove_obj_task, 10000, LV_TASK_PRIO_LOWEST, meme_img); // Create a task to remove the meme image after 10 seconds

  }
 
}

/**
 * @brief  Remove a specific object from a given event. Can be used to remove an object 
 * on the screen when an event has happened.
 * @param  lv_obj_t object_name
 * @param  lv_event_t event_name
 * @return void
 */
static void event_handler(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_VALUE_CHANGED) {
    lv_obj_del(obj);
  }
}

/**
 * @brief  Remove the object from a given task. Can be used to remove an object 
 * on the screen. 
 * @param  lv_task_t task_name
 * @return void
 */
void remove_obj_task(lv_task_t *task) {
  lv_obj_t *obj = (lv_obj_t *)task->user_data; // Get the object from the task data
  lv_obj_del(obj); // Delete the object
  lv_task_del(task); // Delete the task
}

/**
 * @brief  Checks if the button on TTGO T-Watch has been pressed. If pressed, touchscreen 
 * is set to sleep and the watch is powered off. Wakeup source is set to touchscreen but 
 * can be changed to the button as well. 
 * @param  void 
 * @return void
 */
void checkButtonPress() {
  if (irq) {
    irq = false;
    watch->power->readIRQ();
    if (watch->power->isPEKShortPressIRQ()) {
      saveVariables();
      // Clean power chip irq status
      watch->power->clearIRQ();

      // Set  touchscreen sleep
      watch->displaySleep();

      watch->powerOff();

      //Set all channel power off
      watch->power->setPowerOutPut(AXP202_LDO3, false);
      watch->power->setPowerOutPut(AXP202_LDO4, false);
      watch->power->setPowerOutPut(AXP202_LDO2, false);
      watch->power->setPowerOutPut(AXP202_EXTEN, false);
      watch->power->setPowerOutPut(AXP202_DCDC2, false);

      // // TOUCH SCREEN  Wakeup source
      // esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);

      // BUTTON Wakeup source
      esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);

      esp_deep_sleep_start();
    }
    watch->power->clearIRQ();
  }
}
