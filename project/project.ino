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
  {"Time", "Transport", "Destination", "Delay"},
  {"10:05", "Bus 1", "Campus Grasvik", "0 min"},
  {"10:12", "Bus 2", "Amiralen", "3 min",},
  {"10:17", "Train", "Malmo C", "2 min"}
};

const char* campusGrasvik[][4] = {
  {"Time", "Transport", "Destination", "Delay"},
  {"10:07", "Bus 1", "Centralstationen", "2 min"},
  {"10:16", "Bus 4", "Bergasa", "0 min"}
};

const char* amiralen[][4] = {
  {"Time", "Transport", "Destination", "Delay"},
  {"10:09", "Bus 2", "Centralstationen", "0 min"},
  {"10:18", "Bus 4", "Bergasa", "5 min"}
};

const char* fisktorget[][4] = {
  {"Time", "Transport", "Destination", "Delay"},
  {"10:11", "Bus 3", "Centralstationen", "1 min"},
  {"10:19", "Bus 2", "Amiralen", "0 min"},
  {"10:28", "Ferry", "Stenshamn", "3 min"}
};

const char* bergasa[][4] = {
  {"Time", "Transport", "Destination", "Delay"},
  {"10:13", "Bus 4", "Centralstationen", "0 min"},
  {"10:22", "Bus 1", "Campus Grasvik", "4 min"}
};
//Function tile 2: Change between departure tables
static lv_obj_t* departureTable; //dekl av variable som pekar på table

static void updateTable( const char* data[][4],
                      const char* stationName)
{
    
   lv_table_set_cell_value(departureTable, 0, 0,stationName);

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 4; col++) {
      lv_table_set_cell_value(departureTable, row+1, col, data[row][col]);
    }
  }

  lv_obj_center(departureTable);
}
//Tar reda på den valda station användaren väljer
static int selected_transport = 0;

static bool checkTransport(const char* transport) {
  if (selected_transport == 0) {
    return true;
  }

  if (selected_transport == 1) {
    return String(transport).startsWith("Bus");
  }

  if (selected_transport == 2) {
    return String(transport).startsWith("Train");
  }
  
  if (selected_transport == 3) {
    return String(transport).startsWith("Ferry");
  }
}

static void station_changed(lv_event_t* e)
{
    lv_obj_t* dropdown = lv_event_get_target(e);

    int selected = lv_dropdown_get_selected(dropdown); 

    switch (selected) {
        case 0:
            updateTable(centralstationen, "Centralstation");
            break;

        case 1:
            updateTable(campusGrasvik, "Campus Grasvik");
            break;

        case 2:
            updateTable(amiralen, "Amiralen");
            break;

        case 3:
            updateTable(fisktorget, "Fisktorget");
            break;

        case 4:
            updateTable(bergasa, "Bergasa");
            break;
    }
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
    lv_label_set_text(group_label, "Group 16:\n""Diar Sharif\n""Alireza Ghanaati\n""Simon Hugosson\n""Hannah Furehed\n""Casper Barane");
    lv_obj_align(group_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_text_font(group_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(group_label, lv_color_hex(0x808080), 0);
  }

  
  {
   // Tile #2: 
    //Dropdown meny
  lv_obj_t* dropdown_menu = lv_dropdown_create(t2);
  lv_obj_set_width(dropdown_menu, 200);
  lv_dropdown_set_options(dropdown_menu,"Centralstationen\n" "Campus Grasvik\n""Amiralen\n""Fisk Torget\n" "Bergasa\n");
  lv_obj_align(dropdown_menu, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* dropdown_transportype = lv_dropdown_create(t2);
  lv_obj_set_width(dropdown_transportype, 200);
  lv_dropdown_set_text(dropdown_transportype, "Filter");
  lv_dropdown_set_options(dropdown_transportype, "Bus\n" "Train\n" "Ferry\n" "All");
  lv_obj_align(dropdown_transportype, LV_ALIGN_TOP_RIGHT, -10, 10);

  departureTable = lv_table_create(t2); //skapar tabell för tile 2
//callback funk- vid en händelse
  lv_obj_add_event_cb(
    dropdown_menu,
    station_changed,
    LV_EVENT_VALUE_CHANGED,
    NULL
  );
   
  lv_table_set_col_cnt(departureTable,4);
  lv_table_set_row_cnt(departureTable,4);

  updateTable( centralstationen, "Centralstationen");
  
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
