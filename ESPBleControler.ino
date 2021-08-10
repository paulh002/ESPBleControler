/*
 Name:		ESPBleControler.ino
 Created:	7/21/2021 11:43:23 PM
 Author:	paulh
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ESP32Encoder.h>
#include <AceButton.h>
using namespace ace_button;

#include <lvgl.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static const uint32_t screenWidth = 320;
static const uint32_t screenHeight = 240;
const int bottomHeight = 40;
const int topHeight = 25;
const int nobuttons = 6;
const int bottombutton_width = (screenWidth / nobuttons) - 2;
const int bottombutton_width1 = (screenWidth / nobuttons);
const int tab_size_y = 40;

const lv_coord_t x_margin = 5;
const lv_coord_t y_margin = 5;
const int x_number_buttons = 3;
const int y_number_buttons = 3;
const lv_coord_t tab_margin = 20;

int			button_width;
int			button_height;
int			button_width_margin;
int			button_height_margin;

static lv_disp_draw_buf_t	draw_buf;
static lv_color_t			buf[screenWidth * 10];

lv_obj_t*	scr, *bg_page1, *button[20];
lv_style_t	text_style;
lv_style_t	page_style;
lv_style_t	style_btn;

lv_group_t	*button_group[5];
lv_indev_t	*encoder_indev_t;

void button_event_handler(lv_event_t* e);

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char* file, uint32_t line, const char* fn_name, const char* dsc)
{
	Serial.printf("%s(%s)@%d->%s\r\n", file, fn_name, line, dsc);
	Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
	uint32_t w = (area->x2 - area->x1 + 1);
	uint32_t h = (area->y2 - area->y1 + 1);

	tft.startWrite();
	tft.setAddrWindow(area->x1, area->y1, w, h);
	tft.pushColors((uint16_t*)&color_p->full, w * h, true);
	tft.endWrite();

	lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data)
{
	uint16_t touchX, touchY;

	bool touched = tft.getTouch(&touchX, &touchY, 600);

	if (!touched)
	{
		data->state = LV_INDEV_STATE_REL;
	}
	else
	{
		data->state = LV_INDEV_STATE_PR;

		/*Set the coordinates*/
		data->point.x = touchX;
		data->point.y = touchY;
	}
}



/*-------------------------------------------------------
   Optical Rotary encoder settings (used for frequency)
--------------------------------------------------------*/
#define PULSE_INPUT_PIN 21  // Rotaty Encoder A
#define PULSE_CTRL_PIN  35  // Rotaty Encoder B
#define ROTARY_A      16      
#define ROTARY_B      17
#define ROTARY_PRESS  22

ESP32Encoder    Enc_vfo; 
ESP32Encoder	GuiEncoder;
AceButton       rotary_button(ROTARY_PRESS);
volatile lv_indev_state_t	enc_button_state = LV_INDEV_STATE_REL;

void read_encoder(lv_indev_drv_t* indev, lv_indev_data_t* data)
{
	data->enc_diff = GuiEncoder.getCount();
	GuiEncoder.clearCount();
	data->state = enc_button_state;
	if (data->enc_diff > 0)
		data->enc_diff = 1;
	if (data->enc_diff < 0)
		data->enc_diff = -1;
	return;
}


/* Define the UUID for our Custom Service */
#define SERVICE_UUID        "30f43f3e-ea17-11eb-9a03-0242ac130003"
#define CHARACTERISTIC_UUID "427ffc34-ea17-11eb-9a03-0242ac130003"
//#define serviceID BLEUUID((uint16_t)0x1700)
#define serviceID BLEUUID(SERVICE_UUID)

/* Define our custom characteristic along with it's properties */
BLECharacteristic customCharacteristic(
	BLEUUID(CHARACTERISTIC_UUID),
	BLECharacteristic::PROPERTY_READ |
	BLECharacteristic::PROPERTY_NOTIFY
);

static void rotary_button_eventhandler(AceButton*, uint8_t eventType, uint8_t buttonState)
{
	switch (eventType) {
	case AceButton::kEventLongPressed:
		//ToggleSetup(true);
		//enc_button_state = LV_INDEV_STATE_REL;
		Serial.println("Long press");
		break;
	case AceButton::kEventPressed:
		enc_button_state = LV_INDEV_STATE_PR;
		Serial.println("Pressed");
		break;

	case AceButton::kEventReleased:
		enc_button_state = LV_INDEV_STATE_REL;
		Serial.println("Released");
		break;
	}
}

/* This function handles the server callbacks */
bool deviceConnected = false;
class ServerCallbacks : public BLEServerCallbacks {
	void onConnect(BLEServer* MyServer) {
		deviceConnected = true;
		Serial.println("Client connected");
	};

	void onDisconnect(BLEServer* MyServer) {
		Serial.println("Client disconnected");
		deviceConnected = false;
	}
};

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(115200); 
	pinMode(ROTARY_PRESS, INPUT);
	rotary_button.setEventHandler(rotary_button_eventhandler);
	ESP32Encoder::useInternalWeakPullResistors = NONE;
	GuiEncoder.attachHalfQuad(ROTARY_B, ROTARY_A);
	GuiEncoder.setFilter(1023);
	Enc_vfo.attachHalfQuad(PULSE_INPUT_PIN, PULSE_CTRL_PIN);

	BLEDevice::init("BleControler");

	/* Create the BLE Server */
	BLEServer* MyServer = BLEDevice::createServer();
	MyServer->setCallbacks(new ServerCallbacks());  // Set the function that handles Server Callbacks

	/* Add a service to our server */
	BLEService* customService = MyServer->createService(serviceID); //  A random ID has been selected

	/* Add a characteristic to the service */
	customService->addCharacteristic(&customCharacteristic);  //customCharacteristic was defined above

	/* Add Descriptors to the Characteristic*/
	customCharacteristic.addDescriptor(new BLE2902());  //Add this line only if the characteristic has the Notify property

	BLEDescriptor VariableDescriptor(BLEUUID((uint16_t)0x2901));  /*```````````````````````````````````````````````````````````````*/
	VariableDescriptor.setValue("Controler data");          /* Use this format to add a hint for the user. This is optional. */
	customCharacteristic.addDescriptor(&VariableDescriptor);    /*```````````````````````````````````````````````````````````````*/

	/* Configure Advertising with the Services to be advertised */
	MyServer->getAdvertising()->addServiceUUID(serviceID);

	// Start the service
	customService->start();

	// Start the Server/Advertising
	MyServer->getAdvertising()->start();

	Serial.println("Waiting for a Client to connect...");
	
// setup display and lvgl
	
	lv_init();

#if LV_USE_LOG != 0
	lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

	tft.begin();          /* TFT init */
	tft.setRotation(1); /* Landscape orientation, flipped */

	/*Set the touchscreen calibration data,
	  the actual data for your display can be aquired using
	  the Generic -> Touch_calibrate example from the TFT_eSPI library*/
	uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
	tft.setTouch(calData);

	lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

	/*Initialize the display*/
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	/*Change the following line to your display resolution*/
	disp_drv.hor_res = screenWidth;
	disp_drv.ver_res = screenHeight;
	disp_drv.flush_cb = my_disp_flush;
	disp_drv.draw_buf = &draw_buf;
	lv_disp_t *display = lv_disp_drv_register(&disp_drv);

	/*Initialize the (dummy) input device driver*/
	/*static lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_touchpad_read;
	lv_indev_drv_register(&indev_drv);
	*/

	static lv_indev_drv_t indev_drv_enc;
	lv_indev_drv_init(&indev_drv_enc);
	indev_drv_enc.type = LV_INDEV_TYPE_ENCODER;
	indev_drv_enc.read_cb = read_encoder;
	encoder_indev_t = lv_indev_drv_register(&indev_drv_enc);

	scr = lv_scr_act();
	lv_theme_t* th = lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_CYAN), LV_THEME_DEFAULT_DARK, &lv_font_montserrat_14);
	lv_disp_set_theme(display, th);
	
	lv_style_init(&page_style);
	lv_style_set_radius(&page_style, 0);
	lv_style_set_bg_color(&page_style, lv_color_black());
	lv_obj_add_style(scr, &page_style, 0);

	lv_obj_t* tabview_tab = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, tab_size_y);
	lv_obj_set_pos(tabview_tab, 0, 0);
	lv_obj_set_size(tabview_tab, screenWidth, screenHeight);
	lv_obj_add_style(tabview_tab, &page_style, 0);
	lv_obj_set_style_pad_hor(tabview_tab, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_ver(tabview_tab, 0, LV_PART_MAIN);
	lv_obj_clear_flag(tabview_tab, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* tab1 = lv_tabview_add_tab(tabview_tab, "Settings");
	lv_obj_t* tab2 = lv_tabview_add_tab(tabview_tab, "Band");
	lv_obj_t* tab3 = lv_tabview_add_tab(tabview_tab, "Freq");
	lv_obj_t* tab4 = lv_tabview_add_tab(tabview_tab, "RX");
	lv_obj_t* tab5 = lv_tabview_add_tab(tabview_tab, "TX");
	
	lv_obj_clear_flag(lv_tabview_get_content(tabview_tab), LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ONE);
	//lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview_tab);

	static lv_style_t background_style;

	lv_style_init(&background_style);
	lv_style_set_radius(&background_style, 0);


	button_width_margin = ((screenWidth - tab_margin) / x_number_buttons);
	button_width = ((screenWidth - tab_margin) / x_number_buttons) - x_margin;
	button_height = 50;
	button_height_margin = button_height + y_margin;

	lv_style_init(&style_btn);
	lv_style_set_radius(&style_btn, 10);
	lv_style_set_bg_color(&style_btn, lv_color_make(0x60, 0x60, 0x60));
	lv_style_set_bg_grad_color(&style_btn, lv_color_make(0x00, 0x00, 0x00));
	lv_style_set_bg_grad_dir(&style_btn, LV_GRAD_DIR_VER);
	lv_style_set_bg_opa(&style_btn, 255);
	lv_style_set_border_color(&style_btn, lv_color_make(0x9b, 0x36, 0x36));   // lv_color_make(0x2e, 0x44, 0xb2)
	lv_style_set_border_width(&style_btn, 2);
	lv_style_set_border_opa(&style_btn, 255);
	lv_style_set_outline_color(&style_btn, lv_color_black());
	lv_style_set_outline_opa(&style_btn, 255);

	int				ibutton_x = 0, ibutton_y = 0;

	int x_start = screenWidth - (button_width_margin * x_number_buttons);
	x_start = x_start / 2;
	int y_start = screenHeight - (button_height_margin * y_number_buttons) - tab_size_y;
	y_start = y_start / 2;

	button_group[0] = lv_group_create();
	lv_indev_set_group(encoder_indev_t, button_group[0]);

	for (int i=0; i < 9; i++)
	{ 
		button[i] = lv_btn_create(tab1);
		lv_group_add_obj(button_group[0], button[i]);
		lv_obj_add_style(button[i], &style_btn, 0);
		lv_obj_add_event_cb(button[i], button_event_handler, LV_EVENT_CLICKED, NULL);
		lv_obj_align(button[i], LV_ALIGN_TOP_LEFT, ibutton_x* button_width_margin , ibutton_y* button_height_margin);
		lv_obj_add_flag(button[i], LV_OBJ_FLAG_CHECKABLE);		
		lv_obj_set_size(button[i], button_width, button_height);

		lv_obj_t* lv_label = lv_label_create(button[i]);

		char str[20];
		switch (i)
		{
		case 0:
			strcpy(str, "volume");
			break;
		case 1:
			strcpy(str, "gain");
			break;
		case 2:
			strcpy(str, "agc");
			break;
		case 3:
			strcpy(str, "0.5 Khz");
			break;
		case 4:
			strcpy(str, "1 Khz");
			break;
		case 5:
			strcpy(str, "1.5 Khz");
			break;
		case 6:
			strcpy(str, "2 Khz");
			break;
		case 7:
			strcpy(str, "2.5 Khz");
			break;
		case 8:
			strcpy(str, "3 Khz");
			break;
		case 9:
			strcpy(str, "3.5 Khz");
			break;
		
		default:
			sprintf(str, "%d", i);
			break;
		}
		
		lv_label_set_text(lv_label, str);
		lv_obj_center(lv_label);

		ibutton_x++;
		if (ibutton_x >= x_number_buttons)
		{
			ibutton_x = 0;
			ibutton_y++;
		}
	}
	lv_obj_clear_flag(lv_tabview_get_content(tabview_tab), LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ONE);
}

// the loop function runs over and over again until power down or reset
void loop() {
	rotary_button.check();
	int count = 0; //GuiEncoder.getCount(); 
	//GuiEncoder.clearCount();
	int count_vfo = Enc_vfo.getCount();
	Enc_vfo.clearCount();

	
	if (count != 0 || count_vfo)
	{
		if (deviceConnected) {
			/* Set the value */
			Serial.println("+Count :" + String(count));
			char buffer[80];
			sprintf(buffer, "%d; %d", count_vfo, count);			
			customCharacteristic.setValue((char*)&buffer);
			customCharacteristic.notify();
		}
	}	
	lv_timer_handler(); /* let the GUI do its work */
	delay(5);
}


void button_event_handler(lv_event_t* e)
{

	lv_obj_t* obj = lv_event_get_target(e);
	lv_obj_t* label = lv_obj_get_child(obj, 0L);
	char* ptr = lv_label_get_text(label);

}