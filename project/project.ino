#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>

#include "secrets.h"

LilyGo_Class amoled;

static lv_obj_t* tileview;
static lv_obj_t* t1;
static lv_obj_t* t2;
static lv_obj_t* t1_label;
static lv_obj_t* t2_label;
static bool t2_dark = false;

// Fake departure data
const char* centralstationen[][4] = {
  {"Time", "Bus", "Destination", "Delay"},
  {"10:05", "1", "Campus Grasvik", "0 min"},
  {"10:12", "2", "Amiralen", "3 min"}
};

const char* campusGrasvik[][4] = {
  {"Time", "Bus", "Destination", "Delay"},
  {"10:07", "1", "Centralstationen", "2 min"},
  {"10:16", "4", "Bergasa", "0 min"}
};

const char* amiralen[][4] = {
  {"Time", "Bus", "Destination", "Delay"},
  {"10:09", "2", "Centralstationen", "0 min"},
  {"10:18", "4", "Bergasa", "5 min"}
};

const char* fisktorget[][4] = {
  {"Time", "Bus", "Destination", "Delay"},
  {"10:11", "3", "Centralstationen", "1 min"},
  {"10:19", "2", "Amiralen", "0 min"}
};

const char* bergasa[][4] = {
  
  {"Time", "Bus", "Destination", "Delay"},
  {"10:13", "4", "Centralstationen", "0 min"},
  {"10:22", "1", "Campus Grasvik", "4 min"}
};

// Draw one departure table
static void drawTable(lv_obj_t* parent, const char* data[][4],
                      const char* stationName)
{
  lv_obj_t* table = lv_table_create(parent);
  lv_table_set_col_cnt(table,4);
  lv_table_set_row_cnt(table,4);

  for (int col = 0; col < 3; col++) 
  {
    lv_table_add_cell_ctrl(
      table, 0,col, LV_TABLE_CELL_CTRL_MERGE_RIGHT);
    
  }
  
   lv_table_set_cell_value(table, 0, 0,stationName);

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 4; col++) {
      lv_table_set_cell_value(table, row+1, col, data[row][col]);
    }
  }

  lv_obj_center(table);
}

// Function: Tile #2 Color change
static void apply_tile_colors(lv_obj_t* tile, lv_obj_t* label, bool dark)
{
  // Background
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(
    tile, dark ? lv_color_black() : lv_color_white(), 0);

  // Text
  lv_obj_set_style_text_color(
    label, dark ? lv_color_white() : lv_color_black(), 0);
}

static void on_tile2_clicked(lv_event_t* e)
{
  LV_UNUSED(e);
  t2_dark = !t2_dark;
  apply_tile_colors(t2, t2_label, t2_dark);
}

// Function: Creates UI
static void create_ui()
{
  // Fullscreen Tileview
  tileview = lv_tileview_create(lv_scr_act());
  lv_obj_set_size(
    tileview,
    lv_disp_get_hor_res(NULL),
    lv_disp_get_ver_res(NULL));
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

  // Add two horizontal tiles
  t1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
  t2 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);

  // Tile #1
  {
    // Main page
    t1_label = lv_label_create(t1);
    lv_label_set_text(t1_label, "Public Transport Information & Interaction");
    lv_obj_set_style_text_font(t1_label, &lv_font_montserrat_22, 0);
    lv_obj_center(t1_label);
    apply_tile_colors(t1, t1_label, false);

    // Version Label
    lv_obj_t* version_label = lv_label_create(t1);
    lv_label_set_text(version_label, "Version: 1.1");
    lv_obj_align(version_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x808080), 0);

     // Group Label
    lv_obj_t* group_label = lv_label_create(t1);
    lv_label_set_text(group_label, "Group 16:\n""Diar Sharif\n""Ali Ghanaati\n""Simon Hugosson\n""Hannah Furehed\n""Casper Barane");
    lv_obj_align(group_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(group_label, lv_color_hex(0x808080), 0);
  }

  
  {
   // Tile #2: Centralstationen
  drawTable(t2, centralstationen, "Centralstation");
  
  
  // Tile #3: Campus Gräsvik
  drawTable(
    lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR),
    campusGrasvik, "CampusGrasvik");
    
    

  // Tile #4: Amiralen
  drawTable(
    lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR),
    amiralen, "Amiralen");
     

  // Tile #5: Fisktorget
  drawTable(
    lv_tileview_add_tile(tileview, 4, 0, LV_DIR_HOR),
    fisktorget, "Fisktorget");
     

  // Tile #6: Bergåsa
  drawTable(
    lv_tileview_add_tile(tileview, 5, 0, LV_DIR_HOR),
    bergasa, "Bergasa");
  }
}

// Function: Connects to WIFI
static void connect_wifi()
{
  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < 15000) {
    delay(250);
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected.");
  } else {
    Serial.println("WiFi could not connect (timeout).");
  }
}

// Setup runs once on startup
void setup()
{
  Serial.begin(115200);
  delay(200);

  if (!amoled.begin()) {
    Serial.println("Failed to init LilyGO AMOLED.");
    while (true) delay(1000);
  }

  beginLvglHelper(amoled);

  create_ui();
  connect_wifi();
}

// Loop runs continuously after setup
void loop()
{
  lv_timer_handler();
  delay(5);
}
