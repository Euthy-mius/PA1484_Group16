#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include "stations.h"
#include "secrets.h"

LilyGo_Class amoled;

static lv_obj_t* tileview;
static lv_obj_t* t1;
static lv_obj_t* t2;
static lv_obj_t* t3;
static lv_obj_t* t1_label;
static lv_obj_t* t3_label_wifi = nullptr;

#define DEFAULT_STATION_NAME "Karlskrona Centralstation"
#define ROWS(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_DEPARTURES 10

struct LiveDeparture {
  char time[6];          // "14:30"
  char transport[24];    // "Bus 4", "Train 92", "Ferry 1"
  char destination[40];  // "Centralstationen"
  char status[16];       // "On time", "+3 min", "Cancelled"
};

static LiveDeparture departures[MAX_DEPARTURES];
static int  departureCount = 0;
static bool dataLoaded     = false;

static int currentStation     = 0;   // index into stations[]
static int selected_transport = 0;   // 0 = All, 1 = Bus, 2 = Train, 3 = Ferry

static lv_obj_t* departureTable;
static lv_obj_t* statusLabel;
static lv_obj_t* stationDropdown;

static const uint32_t REFRESH_INTERVAL_MS = 60000;
static uint32_t lastFetchMs    = 0;
static bool fetchRequested = true;

static int findStationByName(const char* name)
{
  /*Description of function:
  Loops through all of the stations in stations.h and returns the index of the exact match
  using stringcompare since == would compare memory address and not text. */

  for (int i = 0; i < STATION_COUNT; i++) {
    if (strcmp(stations[i].name, name) == 0) return i;
  }
  return 0;
}

static void fillStationDropdown()
{
  /*Description of function:
  Fills the station dropdown menu automatically rather than us having to type in all of the stations. 
  Checks how much memory is required, allocates a temporary buffer to build the string, 
  and then use set_options to build the dropdown, and free the buffer so no memory leaks occur. */

  // 1. How much memory is needed? Each name + '\n' (last one gets '\0')
  size_t total = 0;
  for (int i = 0; i < STATION_COUNT; i++) {
    total += strlen(stations[i].name) + 1;
  }

  // 2. Build the string in a temporary buffer
  char* options = (char*)malloc(total);
  if (options == NULL) {
    Serial.println("fillStationDropdown: out of memory");
    return;
  }

  size_t used = 0;
  for (int i = 0; i < STATION_COUNT; i++) {
    size_t len = strlen(stations[i].name);
    memcpy(options + used, stations[i].name, len);
    used += len;
    options[used++] = (i < STATION_COUNT - 1) ? '\n' : '\0';
  }

  // 3. LVGL copies the text, so the buffer can be freed right away
  lv_dropdown_set_options(stationDropdown, options);
  free(options);

  // Dropdown position == index in stations[], so this selects the start station
  lv_dropdown_set_selected(stationDropdown, currentStation);
}

static bool checkTransport(const char* transport) {
  /*Description of function:
  Used in update table to only show relevant transport types when a transport filter is chosen. 
  Either All, Bus, Train or Ferry. */

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
  return false;
}

static void updateTable()
{
  /*Description of function:
  Updates the table in regard of departures that are loaded in from the API, and if there is a
  current transport filter to take into consideration. */

  lv_table_set_cell_value(departureTable, 0, 0, stations[currentStation].name);

  int tableRow = 2;
  for (int i = 0; i < departureCount; i++) {
    const LiveDeparture& d = departures[i];
    if (!checkTransport(d.transport)) continue;

    lv_table_set_cell_value(departureTable, tableRow, 0, d.time);
    lv_table_set_cell_value(departureTable, tableRow, 1, d.transport);
    lv_table_set_cell_value(departureTable, tableRow, 2, d.destination);
    lv_table_set_cell_value(departureTable, tableRow, 3, d.status);
    tableRow++;
  }

  if (tableRow == 2) {
    lv_table_set_cell_value(departureTable, 2, 0, "");
    lv_table_set_cell_value(departureTable, 2, 1, "");
    lv_table_set_cell_value(departureTable, 2, 2,
                            dataLoaded ? "No departures" : "Loading...");
    lv_table_set_cell_value(departureTable, 2, 3, "");
    tableRow++;
  }

  lv_table_set_row_cnt(departureTable, tableRow);
}

static void extractClock(const char* isoTime, char* out, size_t outSize)
{
  /*Description of function:
  Help function to show the time in a consistant format since API writes times like 2025-04-01T14:30:00, thus
  changes it to, for example, 14:30. */

  if (strlen(isoTime) >= 16) {
    snprintf(out, outSize, "%.5s", isoTime + 11);
  } else {
    strlcpy(out, "--:--", outSize);
  }
}

static const char* modeName(const char* mode)
{
  /*Description of function:
  Translates the API's transport types to our names used in the project, i.e "Bus", "Train" and "Ferry". */

  if (strcmp(mode, "BUS") == 0)   return "Bus";
  if (strcmp(mode, "TRAIN") == 0) return "Train";
  if (strcmp(mode, "BOAT") == 0)  return "Ferry";
  return mode;
}

static void formatStatus(JsonObject dep, char* out, size_t outSize)
{
  /*Description of function:
  Checks the API fields to determine departure status of the departure, either cancelled, no realtime data, or either a delay or ontime. */

  if (dep["canceled"] | false) {
    strlcpy(out, "Cancelled", outSize);
    return;
  }
  if (!(dep["is_realtime"] | false)) {
    strlcpy(out, "Scheduled", outSize);
    return;
  }

  int delayMin = (dep["delay"] | 0) / 60;
  if (delayMin > 0)      snprintf(out, outSize, "+%d min", delayMin);
  else if (delayMin < 0) snprintf(out, outSize, "%d min early", -delayMin);
  else                   strlcpy(out, "On time", outSize);
}

static bool fetchDepartures(const char* areaId, char* updatedAt, size_t updatedAtSize)
{
  /*Description of function:
  The API call function, broken down in different parts, listed and explained below.*/

  // Part 1: Check Wifi and setup connection.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("fetchDepartures: no WiFi");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);

  // Part 2: Build the URL and send the request. 
  String url = String("https://realtime-api.trafiklab.se/v1/departures/") + areaId + "?key=" + TRAFIKLAB_API_KEY;

  if (!http.begin(client, url)) {
    Serial.println("fetchDepartures: http.begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("fetchDepartures: HTTP %d\n", httpCode);
    http.end();
    return false;
  }

  // Part 3: A filter to only keep the fields we need for our departure table.
  JsonDocument filter;
  filter["timestamp"] = true;
  filter["departures"][0]["scheduled"]   = true;
  filter["departures"][0]["delay"]       = true;
  filter["departures"][0]["canceled"]    = true;
  filter["departures"][0]["is_realtime"] = true;
  filter["departures"][0]["route"]["designation"]    = true;
  filter["departures"][0]["route"]["name"]           = true;
  filter["departures"][0]["route"]["transport_mode"] = true;
  filter["departures"][0]["route"]["direction"]      = true;

  // Part 4: Parse the response using the filter.
  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("fetchDepartures: JSON error %s\n", err.c_str());
    return false;
  }

  // Part 5: Copy the data into the array, with: Time, Transport type, Destination, Status. Only copies the amount of MAX_DEPARTURES to
  // not show every departure in the response.
  departureCount = 0;
  for (JsonObject dep : doc["departures"].as<JsonArray>()) {
    if (departureCount >= MAX_DEPARTURES) break;

    LiveDeparture& d = departures[departureCount];

    extractClock(dep["scheduled"] | "", d.time, sizeof(d.time));

    const char* line = dep["route"]["designation"] | (dep["route"]["name"] | "");
    const char* mode = dep["route"]["transport_mode"] | "";
    snprintf(d.transport, sizeof(d.transport), "%s %s", modeName(mode), line);

    strlcpy(d.destination, dep["route"]["direction"] | "", sizeof(d.destination));

    formatStatus(dep, d.status, sizeof(d.status));

    departureCount++;
  }
  
  // Part 6: Show a timestamp of when the API was last updated.
  extractClock(doc["timestamp"] | "", updatedAt, updatedAtSize);

  Serial.printf("fetchDepartures: %d departures\n", departureCount);
  return true;
}

static void refreshFromApi()
{
  /*Description of function:
  Function that shows status of the API call, and is the one function that is called from when interacting with the screen,
  to refresh the data shown. This happens, right after startup, when the user picks a new station, and every 60 seconds, 
  as shown further down in the void loop(). */

  lv_label_set_text(statusLabel, "Updating... | Data from Trafiklab.se");
  lv_refr_now(NULL);

  char updatedAt[6];
  if (fetchDepartures(stations[currentStation].areaId, updatedAt, sizeof(updatedAt))) { // Fetch the data and check if it worked.
    dataLoaded = true;
    lv_label_set_text_fmt(statusLabel, "Updated %s | Data from Trafiklab.se", updatedAt);
  } else {
    lv_label_set_text(statusLabel, "Update failed | Data from Trafiklab.se");
  }
  updateTable();
}

static void station_changed(lv_event_t* e)
{
  /*Description of function:
  Takes the input from the dropdown, sets the currentStation to the index of stations[], and calls for update table with new data from
  the API call. */

  lv_obj_t* dropdown = lv_event_get_target(e);
  currentStation = lv_dropdown_get_selected(dropdown);  // position == stations[] index

  departureCount = 0;
  dataLoaded = false;
  updateTable();

  fetchRequested = true;
}

static void filter_changed(lv_event_t* e)
{
  /*Description of function:
  Checks if a transport filter has changed and then updates the table with that filter. */

  lv_obj_t* dropdown = lv_event_get_target(e);
  selected_transport = lv_dropdown_get_selected(dropdown);
  updateTable();
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

// Function: Tile #3 Wifi status update
static void update_wifi_status(lv_timer_t* timer)
{
  static int last_state = -1;
  int connected = (WiFi.status() == WL_CONNECTED);
  if (connected == last_state) return;
  last_state = connected;

  if (connected) {
    lv_label_set_text(t3_label_wifi, LV_SYMBOL_WIFI " Connected");
    lv_obj_set_style_text_color(t3_label_wifi, lv_palette_darken(LV_PALETTE_GREEN, 2), 0);
  } else {
    lv_label_set_text(t3_label_wifi, LV_SYMBOL_WIFI " Not Connected");
    lv_obj_set_style_text_color(t3_label_wifi, lv_palette_darken(LV_PALETTE_RED, 2), 0);
  }
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

  // Add three horizontal tiles
  t1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
  t2 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
  t3 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);

 
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
  departureTable = lv_table_create(t2);
  lv_table_set_col_cnt(departureTable, 4);
  lv_table_set_row_cnt(departureTable, 3);

  for (int col = 0; col < 3; col++) {
    lv_table_add_cell_ctrl(departureTable, 0, col, LV_TABLE_CELL_CTRL_MERGE_RIGHT);
  }

  lv_table_set_cell_value(departureTable, 1, 0, "Time");
  lv_table_set_cell_value(departureTable, 1, 1, "Line");
  lv_table_set_cell_value(departureTable, 1, 2, "Destination");
  lv_table_set_cell_value(departureTable, 1, 3, "Status");

  lv_obj_align(departureTable, LV_ALIGN_TOP_MID, 0, 70);

  statusLabel = lv_label_create(t2);
  lv_label_set_text(statusLabel, "Connecting... | Data from Trafiklab.se");
  lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x808080), 0);
  lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -10);

  updateTable();
  }

  {
  // Tile #3:
  // Settings page

  lv_obj_t* title = lv_label_create(t3);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 20);

  lv_obj_t* station_label = lv_label_create(t3);
  lv_label_set_text(station_label, "Station");
  lv_obj_set_style_text_color(station_label, lv_color_hex(0x808080), 0);
  lv_obj_align(station_label, LV_ALIGN_TOP_LEFT, 20, 72);

  lv_obj_t* transport_label = lv_label_create(t3);
  lv_label_set_text(transport_label, "Transport");
  lv_obj_set_style_text_color(transport_label, lv_color_hex(0x808080), 0);
  lv_obj_align(transport_label, LV_ALIGN_TOP_LEFT, 310, 72);
  
  stationDropdown = lv_dropdown_create(t3);
  lv_obj_set_width(stationDropdown, 270);
  lv_obj_add_event_cb(stationDropdown, station_changed, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_align(stationDropdown, LV_ALIGN_TOP_LEFT, 20, 94);
  fillStationDropdown();

  lv_obj_t* dropdown_transportype = lv_dropdown_create(t3);
  lv_obj_set_width(dropdown_transportype, 270);
  lv_dropdown_set_options(dropdown_transportype, "All\nBus\nTrain\nFerry");
  lv_obj_add_event_cb(dropdown_transportype, filter_changed, LV_EVENT_VALUE_CHANGED, NULL); 
  lv_obj_align(dropdown_transportype, LV_ALIGN_TOP_LEFT, 310, 94);

  // Wifi status label
  t3_label_wifi = lv_label_create(t3);
  lv_obj_set_style_text_font(t3_label_wifi, &lv_font_montserrat_16, 0);
  lv_obj_align(t3_label_wifi, LV_ALIGN_BOTTOM_LEFT, 20, -20);
  lv_timer_create(update_wifi_status, 1000, nullptr);
  
  }
}

// Function: Connects to WIFI
static void connect_wifi()
{
  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
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

  currentStation = findStationByName(DEFAULT_STATION_NAME);
  Serial.printf("Loaded %d stations, starting at %s\n", STATION_COUNT, stations[currentStation].name);

  create_ui();
  connect_wifi();
}

// Loop runs continuously after setup
void loop()
{
  lv_timer_handler();

  if (fetchRequested || millis() - lastFetchMs >= REFRESH_INTERVAL_MS) {
    fetchRequested = false;
    lastFetchMs = millis();
    refreshFromApi();
  }

  delay(5);
}
