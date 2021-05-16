/* Koden i dette programmet er lettere modifisert og orginalt skrivet av: 
 *robotzero1
 *28.02.21
 *ESP32-CAM Access Control System
 *Kildekode
 *https://github.com/robotzero1/esp32cam-access-control */

#include <ArduinoWebsockets.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "camera_index.h"
#include "Arduino.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"
#include "string.h";
#include <CircusESP32Lib.h>

//CoT og WiFi oppkoblingvariabler
char ssid[] = "Speed"; //nettverks navn
char password[] = "TryCatchMyPassword"; //nettverks passord
char server[] = "www.circusofthings.com"; //server for tilkobling
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1Mjc1In0.UXcypxoNDC-05QVKc2CSNaQFXqwZbpw9C4CEhTX9aH0"; //bruker token
char person_id_key[] = "23832"; //signal nøkkel 

CircusESP32Lib circusESP32(server,ssid,password);

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7
#define CAMERA_MODEL_AI_THINKER
#define SECOND 1000
#include "camera_pins.h"


using namespace websockets;
WebsocketsServer socket_server; //Oppsett av av websocket for komunikasjon mellom nettleser og ESP32

camera_fb_t * fb = NULL;

//Variabler for ansiktsgjennkjenning:
#define signal_pin 2 //signalpinne for styring av servo knytt til ESP32 ved inngangsdør 
long current_millis;
long last_detected_millis = 0;
int person_id = 0; //verdi brukt til å identifisere hvilket romnummer hver person i kollketivet hører til 
int interval = 5*SECOND; //tid døra skal stå åpen i sekund
unsigned long door_opened_millis = 0;
bool face_recognised = false;

//Variabler for beboere
char* BEBOER1;
char* BEBOER2;
char* BEBOER3;
char* BEBOER4;
char* BEBOER5;
char* BEBOER6;              

void app_facenet_main();
void app_httpserver_init();

typedef struct
{
  uint8_t *image;
  box_array_t *net_boxes;
  dl_matrix3d_t *face_id;
} http_img_process_result;

/*Ansiktsgjennkejnning instillinger*/
static inline mtmn_config_t app_mtmn_config()
{
  mtmn_config_t mtmn_config = {0};
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;
  return mtmn_config;
}
mtmn_config_t mtmn_config = app_mtmn_config();

face_id_name_list st_face_list;
static dl_matrix3du_t *aligned_face = NULL;

httpd_handle_t camera_httpd = NULL;

typedef enum
{
  START_STREAM,
  START_DETECT,
  SHOW_FACES,
  START_RECOGNITION,
  START_ENROLL,
  ENROLL_COMPLETE,
  DELETE_ALL,
} en_fsm_state;
en_fsm_state g_state;

typedef struct
{
  char enroll_name[ENROLL_NAME_LEN];
} httpd_resp_value;

httpd_resp_value st_name;

void setup() {
  Serial.begin(115200); //starter printing til serial monitor
  circusESP32.begin(); //starter CoT element
  Serial.setDebugOutput(true);

  digitalWrite(signal_pin, LOW);
  pinMode(signal_pin, OUTPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  //Initialiserer med høge spesifikasjoner for å på førehand fordele store buffere
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  //Kamera initialisering
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera initialisering feilet med kode: 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("kobler til..");
  }
  Serial.println("Tilkoblet");

  app_httpserver_init();
  app_facenet_main();
  socket_server.listen(82);

  Serial.print("IP-adresse for å koble til:");
  Serial.print(WiFi.localIP());
}

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

httpd_uri_t index_uri = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = index_handler,
  .user_ctx  = NULL
};

void app_httpserver_init ()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }
}

/*Laster inn eksisterende ansikt fra ESP32 ved oppstart*/
void app_facenet_main()
{
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
  read_face_id_from_flash_with_name(&st_face_list);
}

static inline int do_enrollment(face_id_name_list *face_list, dl_matrix3d_t *new_id) //Registrer nye ansikt og lagerer de til ESP32
{
  ESP_LOGD(TAG, "START ENROLLING");
  int left_sample_face = enroll_face_id_to_flash_with_name(face_list, new_id, st_name.enroll_name);
  ESP_LOGD(TAG, "Face ID %s Enrollment: Sample %d",
           st_name.enroll_name,
           ENROLL_CONFIRM_TIMES - left_sample_face);
  return left_sample_face;
}

/*Bruker websockets til å sende listen av ansikt til nettleseren*/
static esp_err_t send_face_list(WebsocketsClient &client)
{
  client.send("delete_faces"); //sletter alle ansikt
  face_id_node *head = st_face_list.head;
  char add_face[64];
  for (int i = 0; i < st_face_list.count; i++) //blar gjennom nåverende ansikt
  {
    sprintf(add_face, "listface:%s", head->id_name);
    client.send(add_face); //sender ansikt til nettleser
    head = head->next;
  }
}
 /*Sletter alle ansikt fra nettleseren og ESP32en*/
static esp_err_t delete_all_faces(WebsocketsClient &client)
{
  delete_face_all_in_flash_with_name(&st_face_list);
  client.send("delete_faces");
}

/*Når ulike komandoer blir sendt fra nettleseren blir de her realisert slik at tilpassande variabler på ESP32en blir satt og ulike funksjoner blir tilkallet*/
void handle_message(WebsocketsClient &client, WebsocketsMessage msg)
{
  if (msg.data() == "stream") {
    g_state = START_STREAM;
    client.send("STREAMING");
  }
  if (msg.data() == "detect") {
    g_state = START_DETECT;
    client.send("DETECTING");
  }
  if (msg.data().substring(0, 8) == "capture:") {
    g_state = START_ENROLL;
    char person[FACE_ID_SAVE_NUMBER * ENROLL_NAME_LEN] = {0,};
    msg.data().substring(8).toCharArray(person, sizeof(person));
    memcpy(st_name.enroll_name, person, strlen(person) + 1);
    client.send("CAPTURING");
  }
  if (msg.data() == "recognise") {
    g_state = START_RECOGNITION;
    client.send("RECGONISING");
  }
  if (msg.data().substring(0, 7) == "remove:") {
    char person[ENROLL_NAME_LEN * FACE_ID_SAVE_NUMBER];
    msg.data().substring(7).toCharArray(person, sizeof(person));
    delete_face_id_in_flash_with_name(&st_face_list, person);
    send_face_list(client); //resetter ansikt i nettleseren
  }
  if (msg.data() == "delete_all") {
    delete_all_faces(client);
  }
}

void open_door(WebsocketsClient &client) {
  if (digitalRead(signal_pin) == LOW) {
    digitalWrite(signal_pin, HIGH); //setter signalpinnen for åpening av døra til høg
    Serial.println("Dør opplåst");
    client.send("door_open");
    door_opened_millis = millis(); //tidspunkt døra ble ulåst
  }
}

void loop() {
  auto client = socket_server.accept();
  client.onMessage(handle_message);
  dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, 320, 240, 3);
  http_img_process_result out_res = {0};
  out_res.image = image_matrix->item;

  send_face_list(client);
  client.send("STREAMING");

  while (client.available()) { //når nettleseren er tilkobla til går denne while løkka evig 
    client.poll();

    if (millis() - interval > door_opened_millis) { //hvis nåverande tid trekt fra hvor lenge døra skal stå åpen er større enn tida fra da døra ble ulåst skal døra skal låse seg
      digitalWrite(signal_pin, LOW); //sender signal om at signal pinnen skal vere lav noe som fører til at døra låser seg.
      if (person_id != 0) {
        circusESP32.write(person_id_key,person_id,token); //skriver person_id verdi til CoT
        person_id = 0; //setter person_id tilbake til utgangsverdi som indikerer at ingen nye beboerer har ankommet kollektivet
      }
    }

    fb = esp_camera_fb_get();

    if (g_state == START_DETECT || g_state == START_ENROLL || g_state == START_RECOGNITION)
    {
      out_res.net_boxes = NULL;
      out_res.face_id = NULL;

      fmt2rgb888(fb->buf, fb->len, fb->format, out_res.image);

      out_res.net_boxes = face_detect(image_matrix, &mtmn_config);

      if (out_res.net_boxes)
      {
        if (align_face(out_res.net_boxes, image_matrix, aligned_face) == ESP_OK)
        {

          out_res.face_id = get_face_id(aligned_face);
          last_detected_millis = millis();
          if (g_state == START_DETECT) {
            client.send("FACE DETECTED");
          }

          if (g_state == START_ENROLL)
          {
            int left_sample_face = do_enrollment(&st_face_list, out_res.face_id);
            char enrolling_message[64];
            sprintf(enrolling_message, "SAMPLE NUMBER %d FOR %s", ENROLL_CONFIRM_TIMES - left_sample_face, st_name.enroll_name);
            client.send(enrolling_message);
            if (left_sample_face == 0)
            {
              ESP_LOGI(TAG, "FACE CAPTURED FOR %s", st_face_list.tail->id_name);
              g_state = START_STREAM;
              char captured_message[64];
              sprintf(captured_message, "Enrolled Face ID: %s", st_face_list.tail->id_name);
              client.send(captured_message);
              send_face_list(client);

            }
          }

          if (g_state == START_RECOGNITION  && (st_face_list.count > 0))
          {
            face_id_node *f = recognize_face_with_name(&st_face_list, out_res.face_id);
            if (f)
            {
              char recognised_message[64];
              sprintf(recognised_message, "DOOR OPEN FOR %s", f->id_name);
              
              //Sjekker hvilken beboer i kollektivet som blir gjenkjent og gir hver en unik verdi fra 1-6.
              if (strcmp(f->id_name,"BEBOER1") == 0) {
                person_id = 1;
              }
              if (strcmp(f->id_name,"BEBOER2") == 0) {
                person_id = 2;
              }
              if (strcmp(f->id_name,"BEBOER3") == 0) {
                person_id = 3;
              }
              if (strcmp(f->id_name,"BEBOER4") == 0) {
                person_id = 4;
              }
              if (strcmp(f->id_name,"BEBOER5") == 0) {
                person_id = 5;
              }
              if (strcmp(f->id_name,"BEBOER6") == 0) {
                person_id = 6;
              }
              //sender person_id til CoT for videre avlesning av ESP32 ved inngangsdør
              circusESP32.write(person_id_key,person_id,token);

              //Sender signal om at døra skal åpnes
              open_door(client);
              client.send(recognised_message);
            }
            else
            {
              client.send("FACE NOT RECOGNISED");
            }
          }
          dl_matrix3d_free(out_res.face_id);
        }

      }
      else
      {
        if (g_state != START_DETECT) {
          client.send("NO FACE DETECTED");
        }
      }

      if (g_state == START_DETECT && millis() - last_detected_millis > 500) {//søker etter ansikt, men ingen ansikt funnet
        client.send("DETECTING");      
      }
    }
    client.sendBinary((const char *)fb->buf, fb->len); //sender bilde fra ESP32 til nettleser 
    esp_camera_fb_return(fb);
    fb = NULL;
  }
}
