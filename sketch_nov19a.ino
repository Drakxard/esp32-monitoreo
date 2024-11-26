#include <LittleFS.h>
#include "BluetoothSerial.h"
#include <RTClib.h>
#define PIN_BOTON 13 
RTC_DS3231 rtc;

BluetoothSerial SerialBT;

unsigned long actualmillis = 0;
// Debounce
volatile bool boton_presionado = false;
unsigned long ant_millis = 0;
const unsigned long tiempo_antirrebote = 200;

int c = 0;
const int intervalotem = 1000;
uint8_t deepSleep = 5;
bool estado_activo = false;
bool fin=false;
enum State {
  esperar=6, comenzar=8, estado0=0, estado1=1, estado2=2, estado3, interactua, dormir
};

union SensorData {
  struct {
    uint16_t temp1;
    uint16_t temp2;
    uint8_t hum1;
    uint8_t hum2;
    uint8_t minuto;
    uint8_t hora;
    uint8_t dia;
    uint8_t mes;
    uint16_t anio;
  } values;
  uint8_t buffer[12];
};
State state = comenzar;
SensorData data;

int compost_actual = 0;
int etapa_actual = 0;

String path = "";


void leerDeepSleepConfig() {
  if (LittleFS.exists("/deep_sleep.bin")) {
    File file = LittleFS.open("/deep_sleep.bin", "r");
    if (file) {
      deepSleep = file.read();
      file.close();
    }
  }
}

void Suspension(uint8_t minutos, bool fin) {
if (!fin){
  if (minutos < 1) minutos = 1;
  if (minutos > 180) minutos = 180; 

  // Calcular el tiempo en microsegundos para el RTC interno
  uint64_t tiempo_microsegundos = (uint64_t)minutos * 60 * 1000000;

  // Configurar el tiempo de suspensión
  esp_sleep_enable_timer_wakeup(tiempo_microsegundos);
  Serial.printf("Configurado para dormir %d minuto(s)...\n", minutos);
   }
  // Configurar el botón como fuente de despertador
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0); // Nivel bajo en GPIO13
  Serial.println("Configurado para despertar con el botón en GPIO 13...");

  // Iniciar suspensión
  Serial.println("Entrando en modo de suspensión...");
  esp_deep_sleep_start();
}
void AlDespertar() {
  state=interactua;
}

void IRAM_ATTR PulsacionBoton() {
  unsigned long tiempo_actual = millis();
  if (tiempo_actual - ant_millis > tiempo_antirrebote) {  // Antirrebote
    boton_presionado = true;
    estado_activo=true;
    ant_millis = tiempo_actual;
  }
}


void manejarCompostActual() {
  File file = LittleFS.open("/compost_actual.bin", FILE_READ);

  if (!file || file.size() == 0) {
    file.close();
    file = LittleFS.open("/compost_actual.bin", FILE_WRITE);
    if (file) {
      compost_actual = 1;
      file.write((uint8_t*)&compost_actual, sizeof(compost_actual));
      file.close();
    }
  } else {
    file.read((uint8_t*)&compost_actual, sizeof(compost_actual));
    file.close();
  }
}

void manejarEtapaActual(State estado) {
  // Abrir el archivo en modo de lectura/escritura
  File file = LittleFS.open("/etapa_actual.bin", FILE_WRITE);
  if (file) {
    // Escribir el estado actual en el archivo
    etapa_actual = static_cast<int>(estado);
    file.write((uint8_t*)&etapa_actual, sizeof(etapa_actual));
    file.close();
    Serial.println("Estado actual guardado exitosamente.");
  } else {
    Serial.println("Error al abrir el archivo para guardar el estado actual.");
  }
}

String crearArchivoCompostEtapa() {
  String filename = "/" + String(compost_actual) + "_" + String(etapa_actual) + ".bin";

  if (!archivoRegistrado(filename)) {  // Verifica si el archivo no está registrado
    if (!LittleFS.exists(filename)) {  // Verifica si el archivo no existe físicamente
      File file = LittleFS.open(filename, FILE_WRITE);
      if (file) {
        SerialBT.println("Archivo creado: " + filename);
        file.close();
      } else {
        SerialBT.println("Error al crear archivo: " + filename);
        return "";
      }
    } else {
      SerialBT.println("El archivo ya existe físicamente: " + filename);
    }

    // Registrar el archivo en path_registro3.bin
    File registros = LittleFS.open("/path_registro3.bin", FILE_APPEND);
    if (registros) {
      registros.println(filename);
      registros.close();
      SerialBT.println("Registrado en /path_registro3.bin: " + filename);
    } else {
      SerialBT.println("Error al registrar en path_registro3.bin");
    }
  } else {
    SerialBT.println("El archivo ya está registrado: " + filename);
  }

  return filename;
}

uint8_t leerEtapaActual() {
  File file = LittleFS.open("/etapa_actual.bin", FILE_READ);
  if (file) {
    uint8_t etapa_actual = 0;
    file.read((uint8_t*)&etapa_actual, sizeof(etapa_actual)); 
    file.close();
    Serial.println("Estado actual leído exitosamente.");
    return static_cast<uint8_t>(etapa_actual);
  } else {
    Serial.println("Error al abrir el archivo para leer el estado actual.");
  }
  return 0;
}

bool archivoRegistrado(const String& filename) {
  File registros = LittleFS.open("/path_registro3.bin", FILE_READ);
  if (!registros) {
    Serial.println("No se pudo abrir path_registro3.bin para lectura.");
    return false;
  }

  while (registros.available()) {
    String linea = registros.readStringUntil('\n');
    linea.trim();  // Remover saltos de línea y espacios extra
    if (linea == filename) {
      registros.close();
      return true;  // El archivo ya está registrado
    }
  }

  registros.close();
  return false;  // El archivo no está registrado
}



//-------------------------------------------------
void guardarDatos(SensorData data, String path) {
if(compost_actual>1){  
  File file = LittleFS.open(path.c_str(), FILE_WRITE);

  if (file) {
    file.write(data.buffer, sizeof(data.buffer));
    file.close();
    Serial.println("Datos agregados.");
  } else {
    Serial.println("Error al abrir el archivo para escritura.");
  }
}else{  File file = LittleFS.open(path.c_str(), FILE_APPEND);

  if (file) {
    file.write(data.buffer, sizeof(data.buffer));
    file.close();
    Serial.println("Datos agregados.");
  } else {
    Serial.println("Error al abrir el archivo para escritura.");
  }
  }

}


void interactua_func() {

  if (SerialBT.available()) {
    String command = SerialBT.readString();
    command.trim();
if (command == "estado") {
  if (LittleFS.exists("/name_ble.bin")) {
    File file = LittleFS.open("/name_ble.bin", "r");
    String nameBLE = file.readString();
    file.close();
    SerialBT.println("Nombre Bluetooth actual: " + nameBLE);
  } else {
    SerialBT.println("No se encontró el archivo /name_ble.bin.");
  }

  // Leer y mostrar los umbrales (si existe el archivo)
  if (LittleFS.exists("/umbrales.bin")) {
    File file = LittleFS.open("/umbrales.bin", "r");
    uint8_t u0 = file.read();
    uint8_t u1 = file.read();
    uint8_t u2 = file.read();
    file.close();
    SerialBT.println("Umbrales actuales: " + String(u0) + ", " + String(u1) + ", " + String(u2));
  } else {
    SerialBT.println("No se encontró el archivo /umbrales.bin.");
  }
 if (LittleFS.exists("/deep_sleep.bin")) {
    File file = LittleFS.open("/deep_sleep.bin", "r");
    uint8_t min = file.read();
    SerialBT.println("Deep Sleep actual: " + min);
    file.close();
  } else {
    SerialBT.println("No se encontró el archivo /deep_sleep.bin.");
  }

}
   if (command == "guia") {
    SerialBT.println("Dormir ---> DD");
    SerialBT.println("Leer registros existentes ---> R");
    SerialBT.println("Leer archivo --> R:indice");
    SerialBT.println("C/N ---> C/N:00.0000.00 (eje: 02.3011.29 -> C: 2.3, N: 11.11");
    SerialBT.println("Nueva fecha ---> F:DDMMYYYYHHMM");
    SerialBT.println("Deep Sleep ---> D:Min (eje: 5, 8");
   }
    if(command == "DD"){
       leerDeepSleepConfig();
       SerialBT.print("Deep Sleep de ");
       SerialBT.println(deepSleep);
       SerialBT.println(" min");
       Suspension(deepSleep,fin); 
       }
  
if (command.startsWith("C/N:")) {
  String valores = command.substring(4); // Extraer después de "C/N:"

  // Validar longitud del string
  if (valores.length() >= 10) {
  float carbono = valores.substring(0, 4).toFloat(); // Primeros 4 dígitos para carbono
  float nitrogeno = valores.substring(4, 9).toFloat(); // Próximos 5 dígitos para nitrógeno      
        if (carbono > 0 && nitrogeno > 0) {
            // Calcular el balance C/N
            float relacion = carbono / nitrogeno;
            SerialBT.println("Balance C/N: " + String(relacion, 2));
            if (relacion >= 20.0 && relacion <= 30.0) {
                SerialBT.println("Relación C/N estable. Puede comenzar el compostaje.");
            } else {
                SerialBT.println("Relación C/N inestable. Ajuste las proporciones.");
              if (relacion < 20.0) {
                  SerialBT.println("Agregue más material rico en carbono.");
              } else {  
                  SerialBT.println("Agregue más material rico en nitrógeno.");
                }
              } 
        } else {
          SerialBT.println("Error: Valores deben ser > 0");
        } 
  } else {
    SerialBT.println("Error: Comando incompleto.");
  }
}
if (command.startsWith("D:")) {
  String minutosStr = command.substring(2);
  uint8_t minutos = minutosStr.toInt();
  if (minutos > 0) {
    File file = LittleFS.open("/deep_sleep.bin", "w");
    if (file) {
      file.write(minutos);  // Guardar los minutos en el archivo
      file.close();
      SerialBT.println("Configurado para Deep Sleep por " + String(minutos) + " minutos.");
   } else {
      SerialBT.println("Error al guardar el archivo /deep_sleep.bin.");
    }
  } else {
    SerialBT.println("Error: El valor de minutos debe ser mayor a 0.");
  }
}
   if (command.startsWith("U:")) {                                                 
  String input = command.substring(2);  // Quita la "U:" inicial

  if (input.length() == 6) {  // Asegúrate de que tiene exactamente 6 caracteres
    uint8_t u0 = input.substring(0, 2).toInt();
    uint8_t u1 = input.substring(2, 4).toInt();
    uint8_t u2 = input.substring(4, 6).toInt();

    guardarUmbrales(u0, u1, u2);  // Llama a la función para guardar los umbrales
    SerialBT.println("Asignados: " + String(u0) + ", " + String(u1) + ", " + String(u2));
  } else {
    SerialBT.println("Error: Formato incorrecto. Use UXXYYZZ (6 dígitos).");
  }
} else if (command.startsWith("F:")) {                                             //Mostar fecha anterior
  String dateTime = command.substring(2);  // Extrae la cadena después de "F:"
  
  if (dateTime.length() == 12) {  // Asegura que el formato sea correcto
    String dia = dateTime.substring(0, 2);
    String mes = dateTime.substring(2, 4);
    String anio = dateTime.substring(4, 8);
    String hora = dateTime.substring(8, 10);
    String minuto = dateTime.substring(10, 12);

    String fecha = dia + "/" + mes + "/" + anio;
    String horaCompleta = hora + ":" + minuto + ":00";

    configurarFechaHora(fecha, horaCompleta);  // Configura la fecha y hora
    SerialBT.println("Fecha y hora configuradas: " + fecha + " " + horaCompleta);
  } else {
    SerialBT.println("Error: Formato incorrecto. Usa F:DDMMYYYYHHMM");
  }
}
 else if (command == "R") {
      SerialBT.println("Lista de registros:");
      listarRegistros();
    } else if (command.startsWith("R:")) {
      int seleccion = command.substring(2).toInt();
      gestionarDescargaRegistros(seleccion);
    } else {
      SerialBT.println("Comando no reconocido.");
    }
  }
}
void listarRegistros() {
  File registros = LittleFS.open("/path_registro3.bin", FILE_READ);
  if (!registros) {
    SerialBT.println("No hay registros disponibles.");
    return;
  }

  int index = 0;
  while (registros.available()) {
    String linea = registros.readStringUntil('\n');
    linea.trim();
    if (!linea.isEmpty()) {
      SerialBT.println(String(index) + ") " + linea);
      index++;
    }
  }
  registros.close();
}

void gestionarDescargaRegistros(int seleccion) {
  File registros = LittleFS.open("/path_registro3.bin", FILE_READ);
  if (!registros) {
    SerialBT.println("No se encontró el archivo de registros.");
    return;
  }

  int index = 0;
  String linea;
  while (registros.available()) {
    linea = registros.readStringUntil('\n');
    linea.trim();
    if (index == seleccion) {
      break;
    }
    index++;
  }
  registros.close();

  if (linea.isEmpty() || !LittleFS.exists(linea)) {
    SerialBT.println("Registro no encontrado.");
    return;
  }
  sendFile(linea);
}

void sendFile(String path) {
  File file = LittleFS.open(path.c_str(), "r");
  if (!file) {
    Serial.println("Error: No se pudo abrir el archivo.");
    SerialBT.println("Error: No se pudo abrir el archivo.");
    return;
  }
  if (file.size() == 0) {
    Serial.println("El archivo está vacío.");
    SerialBT.println("El archivo está vacío.");
    file.close();
    return;
  }

  // Leer y enviar datos
  while (file.available() >= sizeof(SensorData)) {
    SensorData data;
    file.read(data.buffer, sizeof(data.buffer));
    
    // Convertir los datos binarios a formato ASCII
    float te1 = data.values.temp1 / 100.0f;
    float te2 = data.values.temp2 / 100.0f;

    String dataString = String(te1) + "," + String(te2) + "," +
                        String(data.values.hum1) + "," + String(data.values.hum2) + "," +
                        String(data.values.minuto) + "," + String(data.values.hora) + "," +
                        String(data.values.dia) + "," + String(data.values.mes) + "," +
                        String(data.values.anio) + "\n";

    // Enviar cada línea de datos ASCII por Bluetooth
    SerialBT.print(dataString);  
    delay(20);  // Ajustable según la capacidad del buffer de Bluetooth
  }

  file.close();
  Serial.println("Archivo enviado en formato ASCII.");
  SerialBT.println("Archivo enviado en formato ASCII.");
}
//-------------------------------------------------------

// Función para almacenar umbrales en un archivo
void guardarUmbrales(uint8_t umbral0, uint8_t umbral1, uint8_t umbral2) {
  File file = LittleFS.open("/umbrales.bin", FILE_WRITE);
  if (file) {
    file.write((uint8_t*)&umbral0, sizeof(umbral0));
    file.write((uint8_t*)&umbral1, sizeof(umbral1));
    file.write((uint8_t*)&umbral2, sizeof(umbral2));
    file.close();
    Serial.println("Umbrales guardados correctamente.");
  } else {
    Serial.println("Error al guardar umbrales.");
  }
}

// Configurar fecha y hora
void configurarFechaHora(const String& fecha, const String& hora) {
  int dia = fecha.substring(0, 2).toInt();
  int mes = fecha.substring(3, 5).toInt();
  int anio = fecha.substring(6, 10).toInt();
  int hora_actual = hora.substring(0, 2).toInt();
  int minuto = hora.substring(3, 5).toInt();
  int segundo = hora.substring(6, 8).toInt();

  if (dia <= 0 || mes <= 0 || anio <= 0 || hora_actual < 0 || minuto < 0 || segundo < 0) {
    Serial.println("Formato de fecha u hora no válido.");
    return;
  }

  DateTime dt(anio, mes, dia, hora_actual, minuto, segundo);
  rtc.adjust(dt);
  Serial.println("Fecha y hora configuradas exitosamente.");
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
if (!LittleFS.begin()) {
    Serial.println("Error al inicializar LittleFS.");
    while (true);
}

  if (SerialBT.begin("ESP32")) {
    Serial.println("Bluetooth inicializado.");
  } else {
    Serial.println("Error al inicializar Bluetooth.");
  }
  if (!rtc.begin()) {
    Serial.println("Error al inicializar RTC.");
  }
  manejarEtapaActual(state);
   Serial.print("Setup: ");
  Serial.println(state);
  manejarCompostActual();
  crearArchivoCompostEtapa();
  int c=0;
  leerDeepSleepConfig();
  
  pinMode(PIN_BOTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BOTON), PulsacionBoton, FALLING);
 if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    AlDespertar();
  }
}
// ---------------- LOOP ----------------
void loop() {
  if(estado_activo){
    state=interactua;
    estado_activo=false;
  }
  actualmillis = millis();
  DateTime time = rtc.now();
  switch (state) {
    case comenzar:
      if (actualmillis - ant_millis > intervalotem) {
        fin=false;
        //Leer ultimo estado y compost, para cargar en ese .bin
        etapa_actual= leerEtapaActual(); 
        if(etapa_actual!= 8){
        c = static_cast<int>(etapa_actual);
        state = static_cast<State>(etapa_actual);
        ant_millis = actualmillis;
        Serial.print(etapa_actual);
        break;
        } else{
          c = 0;
          state = estado0;
          break;
        }
      }
      break;
      case estado0:
        
        if(c == 0 ){
          c++;
          manejarEtapaActual(state);
	     path = crearArchivoCompostEtapa();   //Devulve filename.bin
     		Serial.print(state);
          }

    data.values.temp1 = 0;
    data.values.temp2 = 0;
    data.values.hum1 = 0;
    data.values.hum2 = 0;
    data.values.minuto = 0;
    data.values.hora =0;
    data.values.dia = 0;
    data.values.mes = 0;
    data.values.anio = 0;
    guardarDatos(data,path);

        //  if(data1.temperature > umbral_estado1) {
        if(45 > 43) {
        Serial.println("Fin estado: Temperatura superada");
        state = estado1;
         }
        
        break;
      case estado1:
        if(c == 1 ){
          c++;
          manejarEtapaActual(state);
	     path = crearArchivoCompostEtapa();   //Devulve filename.bin
     		Serial.print(state);
          }
    data.values.temp1 = 1;
    data.values.temp2 = 1;
    data.values.hum1 = 1;
    data.values.hum2 = 1;
    data.values.minuto = 1;
    data.values.hora = 1;
    data.values.dia = 1;
    data.values.mes = 1;
    data.values.anio = 1;
    guardarDatos(data,path);

	//     if(data1.temperature > umbral_estado2) {    
        if(50 > 20) {
        Serial.println("Fin estado: Temperatura superada");
        state = estado2;
         }

        break;
      case estado2:
      if(c == 2){
          c++;
          manejarEtapaActual(state);
	     path = crearArchivoCompostEtapa();   //Devulve filename.bin
     		Serial.print(state);
          }
	
    data.values.temp1 = 2;
    data.values.temp2 = 2;
    data.values.hum1 = 2;
    data.values.hum2 = 2;
    data.values.minuto = 2;
    data.values.hora = 2 ;
    data.values.dia = 2;
    data.values.mes = 2;
    data.values.anio = 2;
    guardarDatos(data,path);

// if(data1.temperature < data2.temperature + 2.0) {
  //cambie a un valor que no se supera
	 if(26 < 22 + 2.0) {
        Serial.println("Compost estabilizado a Temperatura Anbiente");
        state = esperar;    
        fin = true; 
        Serial.print(state);
        }
        //mostrar si permanece aun cuando se interactua
        Serial.print(state);
        delay(2000);
        break;
      case interactua: {
          actualmillis = millis(); // Guardar el tiempo inicial
          const unsigned long tiempoEspera = 60000; // 60 segundos en milisegundos
          while (true) {
                interactua_func();
                    if (millis() - actualmillis >= tiempoEspera) {
                    SerialBT.println("Sesion finalizada");
                    state=comenzar;
                     break; // Salir de la función
                    }
            }

    // Agregar un pequeño retraso para evitar consumir demasiados recursos
    delay(10);
        break;}
      case esperar: {
          File file = LittleFS.open("/compost_actual.bin", FILE_READ);         
          if (file) { 
              file.read((uint8_t*)&compost_actual, sizeof(compost_actual)); 
              file.close();
          } else {
              Serial.println("Error: No se pudo abrir c_a.bin");
          }
          compost_actual++;
          file = LittleFS.open("/compost_actual.bin", FILE_WRITE); 
          if (file) {
              file.write((uint8_t*)&compost_actual, sizeof(compost_actual));
              file.close();
          } else {
              Serial.println("Error: Escritura. c_a.bin");
          }
          leerDeepSleepConfig();
          Serial.println("Entrando en modo esperar...");
          SerialBT.println("Entrando en Deep Sleep por " + String(deepSleep) + " minutos.");
          if()
          Suspension(deepSleep,fin);
          break;
      }
      default:
      break;
  }  
}

