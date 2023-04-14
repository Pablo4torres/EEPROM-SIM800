 //Ubicamos el estado inicial de nuestro tanque con las siguientes condiciones 
  //En este caso hay x condiciones posiblres 

  // EB = 0 , EA = 0 , Contactor = 0 ,    // TODO APAGADO (FALLA)
  // EB = 1 , EA = 1 , Contactor = 1 ,   //TODO ENCENDIDO (FALLA)

  // EB = 0 , EA = 0 , Contactor = 1 , // SOLO CONTACTOR ENCENDIDO -  SOLO BOMBA ENCENDIDA - LLENADO DE TANQUE(FALLA)
  // EB = 0 , EA = 1 , Contactor = 0 , //SOLO EA ENCENDIDO (FALLA EN EL ELCTRODO BAJO) (FALLA)
  // EB = 1 , EA = 0 , Contactor = 0 , // EB ENCENDIDO (TANQUE VACIANDOSE) (TDBN) - ESPERAMOS QUE SE ACTIVE LA BOMBA
  
  // EB = 1 , EA = 1 , Contactor = 0 , // EB Y EA - Tanque lleno bomba apagada (TDBN)
  // EB = 1 , EA = 0 , Contactor = 1 , // EB Y C  - Tanque en llenado (TDBN)
  // EB = 0 , EA = 1 , Contactor = 1 , EA Y C - Electrodo Bajo con falla, Electrodo de arriba encendido y bomba jalando va a tirar el tanque (FALLA)

#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <RTClib.h>
#define I2C_ADDR    0x27
#include <avr/wdt.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(I2C_ADDR, 20, 4 );
SoftwareSerial SIM800l(10, 11);

// Registro de numeros telefonicos
const int totalPhoneNo = 5;
const byte phoneLength = 10;
//memoria total consumida por numero de telefono en EEPROM
const byte totalMemory = (phoneLength * 5) - 1;
//set starting address of each phone number in EEPROM
int offsetPhone[totalPhoneNo] = {0,phoneLength,phoneLength*2,phoneLength*3,phoneLength*4};

// Variable para almacenar mensaje de texto
String textMessage;
String texttext;
String lampState;

 /////////  contadores  ///////
int c;  // remoto off
int c1; // falla
int c2; // todo apagado
int c3; // 
int c4; // llenado NO exitoso

int bandera_llenadocompleto;
int flag = 0;  //bandera funcion TurnOn
int flag1 = 0; //bandera de llenado
int flag2 = 0; // bandera funcion Off
int flag3 = 0; //falla llenado NO exitoso
int lleno = 0; //bandera equipo lleno
int vacio = 0; //bandera equipo vacio



/////////// variables de control ///////////
int contactor = 22; //contacto auxiliar 
int EB= 26;  //Electrodo Bajo
int EA= 28;  //Electrodo Alto 
int relay = 12; // relevador de control

// Crea una variable string para imprimir fecha y hora 
String ano;
String mes;
String dia;
String Hr;
String Min;
String Seg;
String fecha1;
String Hora1;
String nivel;

/*******************************************************************************
   getResponse function
 ******************************************************************************/
boolean getResponse(String expected_answer, unsigned int timeout = 1000, boolean reset = false) {
  boolean flag = false;
  String response = "";
  unsigned long previous;
  //*************************************************************
  for (previous = millis(); (millis() - previous) < timeout;) {
    while (SIM800l.available()) {
      response = SIM800l.readString();
      //----------------------------------------
      //Used in resetSIM800L function
      //If there is some response data
      /*if (response != "") {
        Serial.println(response);
        if (reset == true)
          return true;
      }*/
      //----------------------------------------
      if (response.indexOf(expected_answer) > -1) {
        return true;
      }
    }
  }
  //*************************************************************
  return false;
}

/*******************************************************************************
   setup function
 ******************************************************************************/

void setup() {

  wdt_disable();
    //establecer switch como entrada
  pinMode(EA, INPUT); 
  pinMode(EB, INPUT); 
  pinMode(relay, OUTPUT);
  pinMode(contactor, INPUT);
  
  Serial.begin (19200);   // establemos la comucicacion serial
  SIM800l.begin(19200);

  /////////////////RTC ////////////////
 if (! rtc.begin()) {       // si falla la inicializacion del modulo
 Serial.println("Modulo RTC no encontrado !");  // muestra mensaje de error
 while (1);         // bucle infinito que detiene ejecucion del programa
 }

  lcd.begin(20,4);
  lcd.setBacklight(HIGH);

//-------------------------------------------------------------------
  //LoadStateEEPROM();
//-------------------------------------------------------------------

  // Por defecto el relé está apagado
  digitalWrite(relay, HIGH);
//rtc.adjust(DateTime(__DATE__, __TIME__));  // funcion que permite establecer fecha y horario

  SIM800l.println("AT");
  getResponse("OK", 1000);
  SIM800l.println("AT+CMGF=1");
  getResponse("OK", 1000);
  SIM800l.println("AT+CNMI=2,2,0,0,0");
  getResponse("OK", 1000);  
 //-------------------------------------------------------------------
  Serial.println(GetRegisteredPhoneNumbersList());
//-------------------------------------------------------------------  

  //MensajeWC();
}//end setup


/*******************************************************************************
   Loop Function
 ******************************************************************************/
void loop () {
 
while(SIM800l.available())
  {
    //textMessage = SIM800l.readString();
    String response = SIM800l.readString();    
    //Serial.print(textMessage); 
    Serial.print(response);    
    // si hay un mensaje entrante
    if (response.indexOf("+CMT:") > -1) {
      String myString = getCallerID(response);
      //String phoneLegth =readFromEEPROM();
      String cmd = getMsgContent(response);
    // esta instruccion if se ejecutara si el sim800l recibio el comando "r"
    // y no haya ningun numero de telefono almacenado en la EEPROM
     if (cmd.equals("r")) {
        String admin_phone = readFromEEPROM(offsetPhone[0]);
        if (admin_phone.length() != phoneLength) {
          RegisterPhoneNumber(1, myString, myString);
          break;
        }
        else {
          String text = "Error: No se pudo registrar el número de telefono del administrador";
          Serial.println(text);
          Reply(text, myString);
          break;
        }
      }
    //La accion se realiza solo si el ID de llamadas coincide con cualquiera
    // de los numeros de telefono almacenados en la EEPROM
    if (comparePhone(myString)) {
        doAction(cmd, myString);
      }
      else {
        String text = "Error: Primero registre su número de telefono";
        Serial.println(text);
        Reply(text, myString);
      }
    }      
  } 

  delay(500);
Serial.print("contador equipo OFF por Sms   ");
Serial.println(c);
//Serial.print("contador2 falla contactor OFF   ");
//Serial.println(c1);
Serial.print("contador3 falla todo Apagado   ");
Serial.println(c2);
Serial.print("contador4 llenado No exitoso   ");
Serial.println(c4);

lcd.setCursor(1, 0);
lcd.print("Equipo El Panteon");
 DateTime now = rtc.now();
  lcd.setCursor(2, 1);
    lcd.print("Fecha");
    lcd.print(" ");
    lcd.print(now.day());
    lcd.print('/');
    lcd.print(now.month());
    lcd.print('/');
    lcd.print(now.year());
    lcd.print("  "); 
    
    
int estado_contactor = digitalRead(contactor);

////////////////////// condicion para cuando detecte BOMBA trabajando incialice contador en "0"  TODO APAGADO///////
if (estado_contactor == 0 )
{
  c1=0;
  c2=0;
  c4=0;
  
}

 ///////////////  se ejecutan las funciónes  /////////  
  Reset();
  Tanque_Lleno();
  Tanque_Vaciandose();
  Tanque_Vacio();
  Tanque_Llenandose();
  Tanque_Llenando();
  FallaApagado();
  TurnOff();
  TurnOn();
  //Estado();
  Remoto_Off();
  Dormir(); 

  while (Serial.available())  {
    String response = Serial.readString();
    if (response.indexOf("clear") > -1) {
      Serial.println(response);
      DeletePhoneNumberList();
      Serial.println(GetRegisteredPhoneNumbersList());
      break;
    }
    SIM800l.println(response);
  }
  
}// main loop ends


/*******************************************************************************
   GetRegisteredPhoneNumbersList function:
 ******************************************************************************/
String GetRegisteredPhoneNumbersList() {
  String text = "Lista de numeros registrados: \r\n";
  String temp = "";
  for (int i = 0; i < totalPhoneNo; i++) {
    temp = readFromEEPROM(offsetPhone[i]);
    if (temp == "")
    {
      text = text + String(i + 1) + ". Vacio\r\n";
    }
    else if (temp.length() != phoneLength)
    {
      text = text + String(i + 1) + ".Formato erroneo\r\n";
    }
    else
    {
      text = text + String(i + 1) + ". " + temp + "\r\n";
    }
  }

  return text;
}

/*******************************************************************************
   RegisterPhoneNumber function:
 ******************************************************************************/
void RegisterPhoneNumber(int index, String eeprom_phone, String myString) {
  if (eeprom_phone.length() == phoneLength) {
    writeToEEPROM(offsetPhone[index - 1], eeprom_phone);
    String text = "Telefono" + String(index) + " Registrado: ";
    //text = text + phoneNumber;
    Serial.println(text);
    Reply(text, myString);
  }
  else {
    String text = "Error: El numero de telefono debe ser " + String(phoneLength) + " digitos largos";
    Serial.println(text);
    Reply(text, myString);
  }
}

/*******************************************************************************
   UnRegisterPhoneNumber function:
 ******************************************************************************/
void DeletePhoneNumber(int index, String myString) {
  writeToEEPROM(offsetPhone[index - 1], "");
  String text = "Telefono" + String(index) + " eliminado";
  Serial.println(text);
  Reply(text, myString);
}
/*******************************************************************************
   DeletePhoneNumberList function:
 ******************************************************************************/
void DeletePhoneNumberList() {
  for (int i = 0; i < totalPhoneNo; i++) {
    writeToEEPROM(offsetPhone[i], "");
  }
}

/*******************************************************************************
   doAction function:
   Realiza la accion segun el sms recibido
 ******************************************************************************/
void doAction(String cmd, String myString) {
  

 if (cmd.indexOf("r2=") > -1) {
    RegisterPhoneNumber(2, getNumber(cmd), myString);
  }
  else if (cmd.indexOf("r3=") > -1) {
    RegisterPhoneNumber(3, getNumber(cmd), myString);
  }
  else if (cmd.indexOf("r4=") > -1) {
    RegisterPhoneNumber(4, getNumber(cmd), myString);
  }
  else if (cmd.indexOf("r5=") > -1) {
    RegisterPhoneNumber(5, getNumber(cmd), myString);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else if (cmd == "list") {
    String list = GetRegisteredPhoneNumbersList();
    Serial.println(list);
    Reply(list, myString);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  //else if(cmd == "del=1"){
  //DeletePhoneNumber(1, caller_id);
  //}
  else if (cmd == "del=2") {
    DeletePhoneNumber(2, myString);
  }
  else if (cmd == "del=3") {
    DeletePhoneNumber(3, myString);
  }
  else if (cmd == "del=4") {
    DeletePhoneNumber(4, myString);
  }
  else if (cmd == "del=5") {
    DeletePhoneNumber(5, myString);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM  
 else if (cmd.indexOf("status")>=0)
 {
   Estado(myString);
 }
//MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else if (cmd == "del=all") {
    DeletePhoneNumberList();
    String text = "Todos los numeros de telefono han sido borrados.";
    Serial.println(text);
    Reply(text, myString);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  else {
    String text = "Error: Comando desconocido: " + cmd;
    Serial.println(text);
    Reply(text, myString);
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
}

/*******************************************************************************
   getCallerID function:
 ******************************************************************************/
String getCallerID(String buff) {
  //+CMT: "3123018379","","22/05/20,11:59:15+20"
  //Hello
  unsigned int index, index2;
  index = buff.indexOf("\"");
  index2 = buff.indexOf("\"", index + 1);
  String myString = buff.substring(index + 1, index2);
  myString.trim();
  //Serial.print("index+1= "); Serial.println(index+1);
  //Serial.print("index2= "); Serial.println(index2);
  //Serial.print("length= "); Serial.println(callerID.length());
  Serial.println("Caller ID: " + myString);
  return myString;
}
/*******************************************************************************
   getMsgContent function:
 ******************************************************************************/
String getMsgContent(String buff) {
  //+CMT: "3123018379","","22/05/20,11:59:15+20"
  //Hello
  unsigned int index, index2;
  index = buff.lastIndexOf("\"");
  index2 = buff.length();
  String command = buff.substring(index + 1, index2);
  command.trim();
  command.toLowerCase();
  //Serial.print("index+1= "); Serial.println(index+1);
  //Serial.print("index2= "); Serial.println(index2);
  //Serial.print("length= "); Serial.println(msg.length());
  Serial.println("Command:" + command);
  return command;
}
/*******************************************************************************
   getNumber function:
 ******************************************************************************/
String getNumber(String text) {
  //r=3123018379
  String temp = text.substring(3, 16);
  //Serial.println(temp);
  temp.trim();
  return temp;
}
  
/*******************************************************************************
   Reply function
   Envio de sms
 ******************************************************************************/
void Reply(String text, String myString)
{
//int addrOffset;

 char data[phoneLength + 1];
  for (int i = 0; i < phoneLength; i++)
   {
    //data[i] = EEPROM.read(addrOffset + i);
    String myString = readFromEEPROM(data[i]);
    //String myString = String(data[i]);
    SIM800l.print("AT+CMGF=1\r");
  delay(1000);
  SIM800l.print("AT+CMGS=\"" + myString + "\"\r");
  delay(1000);
  SIM800l.print(text);
  delay(100);
  SIM800l.println((char)26); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(1000);
  Serial.println("SMS enviado satisfactoriamente.");
  delay(20000);

  }
 
 /* //return;
  SIM800l.print("AT+CMGF=1\r");
  delay(100);
  SIM800l.print("AT+CMGS=\"" + caller_id + "\"\r");
  delay(100);
  SIM800l.print(text);
  delay(100);
  SIM800l.println((char)26); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(100);
  Serial.println("SMS enviado satisfactoriamente.");
  delay(5000);*/
}


/*******************************************************************************
   writeToEEPROM function:
   Almacenar los numeros de telefono registrados en la EEPROM
 ******************************************************************************/
void writeToEEPROM(int addrOffset, const String &strToWrite)
{
  //byte phoneLength = strToWrite.length();
  //EEPROM.write(addrOffset, phoneLength);
  for (int i = 0; i < phoneLength; i++)
  {
    EEPROM.write(addrOffset + i, strToWrite[i]);
  }
}

/*******************************************************************************
   readFromEEPROM function:
   Almacenar numeros de telefono en la EEPROM
 ******************************************************************************/
String readFromEEPROM(int addrOffset)
{
  //byte phoneLength = strToWrite.length();
  char data[phoneLength + 1];
  for (int i = 0; i < phoneLength; i++)
  {
    data[i] = EEPROM.read(addrOffset + i);
  }
  data[phoneLength] = '\0';
  return String(data);
}




/*******************************************************************************
   comparePhone function:
   Comparar los numeros de telefono almacenados en la EEPROM
 ******************************************************************************/
boolean comparePhone(String number)
{
  boolean flag = 0;
  String tempPhone = "";
  //--------------------------------------------------
  for (int i = 0; i < totalPhoneNo; i++) {
    tempPhone = readFromEEPROM(offsetPhone[i]);
    if (tempPhone.equals(number)) {
      flag = 1;
      break;
    }
  }
  //--------------------------------------------------
  return flag;
}  

//////////////////funcion sleep arduino /////////////////////////////////
void sleepNow() 
{
  set_sleep_mode(SLEEP_MODE_IDLE); 
  sleep_enable(); 
  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_twi_disable();
  sleep_mode(); 
  sleep_disable(); 
  power_all_enable();
}

//////////FUNCION SLEEP & WAKE UP////////////

void Dormir()
{
 
 if(textMessage.indexOf("WAKEUP")>=0)
  {
    String message = "Sistema Activado "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     //sendSMS(message);
     //sendSMS2(message);
     textMessage = ""; 
   }
if(textMessage.indexOf("SLEEP")>=0)
  {
    String message = "Sistema Desactivado "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     //sendSMS(message);
     //sendSMS2(message);
     textMessage = ""; 
     digitalWrite(relay, HIGH);// Apaga el relé 
     sleepNow();
   }

        DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
}



/////// FUNCION RESET /////
void Reset()
{
  if(textMessage.indexOf("RESET")>=0)
  {
      wdt_enable(WDTO_15MS);
      //delay(200);
}
}


//////////////  Funcion para preguntar ESTADO actual del equipo SMS  //////////////////
void Estado(String myString)
{
  //if(textMessage.indexOf("STATUS")>=0)
  //{
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);
  
    if( estado_EB == 1 && estado_EA == 1 && estado_contactor == 0)
    {
    String text = "Tanque en proceso de Llenado, E.Bajo-OFF  E.Alto-OFF  Bomba-ON, Sistema Estable trabajando correctamente "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     Reply(text, myString);
     //Reply2(message,phoneLegth);
     texttext = ""; 
   }
   else if( estado_EB == 0 && estado_EA == 1 && estado_contactor == 0)
    {
    String text = "Tanque en proceso de Llenado, E.Bajo-ON  E.Alto-OFF  Bomba-ON, Sistema Estable trabajando correctamente  "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     Reply(text, myString);
     //Reply2(message,phoneLegth);
     texttext = ""; 
   }
   else if( estado_EB == 0 && estado_EA == 0 && estado_contactor == 1)
    {
    String text = "Llenado del Tanque correctamente, E.Bajo-ON  E.Alto-ON  Bomba-OFF, Sistema Estable trabajando correctamente  "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     Reply(text, myString);
     //Reply2(message,phoneLegth);
     texttext = ""; 
   }
   else if ( estado_EB == 0 && estado_EA == 1 && estado_contactor == 1)
   { 
    String text = "Tanque vaciandose, E.Bajo-ON  E.Alto-OFF  Bomba-OFF, Sistema Estable trabajando correctamente "  " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     Reply(text, myString);
     //Reply2(message,phoneLegth);
     texttext = ""; 
   } 
 // }
        DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
}


///////////// EB = 0;  EA = 0;  CONTACTOR = 1 TANQUE LLENO - EQUIPO APAGADO POR NIVEL ////////////
void Tanque_Lleno()
{
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

   if( estado_EB == 0 && estado_EA == 0 && estado_contactor == 1 )
   {
    lcd.setCursor(0,2);
    lcd.print("Pump Off EA On EB On");
    Serial.println("TANQUE LLENO - EQUIPO APAGADO POR NIVEL (TODO OKA)");
    digitalWrite(relay, LOW);// Enciende el relé 
    delay(500);
           
    flag = 1; // se desabilita la funcion TurnoOn para evitar falsos mensajes
    bandera_llenadocompleto = 1; // llenado exitoso OKA
    vacio = 0; // se habilita bandera vacio para enviar solo un sms  
       }
}


///////////// EB = 0;  EA = 1;  CONTACTOR = 1  TANQUE EN PROCESO DE VACIADO - ELECTRODO ALTO FUERA DEL AGUA////////////
void Tanque_Vaciandose()
{
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

   if( estado_EB == 0 && estado_EA == 1 && estado_contactor == 1)
   {
    if (bandera_llenadocompleto == 1 && lleno == 0)
    {
    lcd.setCursor(0,2);
    lcd.print("Pump Off EA Of EB On");
    Serial.println("TANQUE EN PROCESO DE VACIADO - ELECTRODO ALTO FUERA DEL AGUA");
    digitalWrite(relay, LOW);// Enciende el relé 
    delay(500);
    lampState = "Equipo Apagado por Nivel E.Bajo-ON  E.Alto-OFF  Bomba-OFF";
          String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          textMessage = "";
          
    flag1 = 0;  // se habilita el llenado      
    flag = 1; // se desabilita la funcion TurnoOn para evitar falsos mensajes
    flag3 = 1; // desabilita bandera llenado NO exitoso
    lleno = 1; // se desabilita bandera de lleno para enviar solo un sms
    vacio = 0; // se habilita bandera vacio para enviar solo un sms   
    }
    else if (flag3 == 0)
    {
      if (c4 == 1750)
      {
         delay(10);           
     Serial.println("ALERTAAAAAAA");
     lampState = "El llenado NO fue completado exitosamente ocurrio una FALLA, cuidar el consumo del Agua  ";
       
       String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          textMessage = "";
         c4=0;
      }
      c4=c4+1;
    }
   }
         DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
}



///////////// EB = 1;  EA = 1;  CONTACTOR = 1  TANQUE TOTALMENTE VACIO- ELECTRODO ALTO Y BAJO FUERA DEL AGUA////////////
void Tanque_Vacio()
{
  
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

  if (flag2 ==0) //bandera off
  {
   if( estado_EB == 1 && estado_EA == 1 && estado_contactor == 1 && flag1 == 0)
   {
    lcd.setCursor(0,2);
    lcd.print("Pump Off EA Of EB Of");
    Serial.println("TANQUE TOTALMENTE VACIO- ELECTRODO ALTO Y BAJO FUERA DEL AGUA");
    delay(500); 
    digitalWrite(relay, HIGH);// Apaga el relé 
    
   }
   }
}


///////////// EB = 1;  EA = 1;  CONTACTOR = 0  TANQUE EN PROCESOS DE LLENADO- CONTACTOR ENERGIZADO (BOMBA OPERANDO) ////////////
void Tanque_Llenandose()
{
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);  
  
 if( estado_EB == 1 && estado_EA == 1 && estado_contactor == 0 && vacio == 0)
{ 
    lcd.setCursor(0,2);
    lcd.print("Pump On EA Of EB Of");  
  
    
    Serial.println("TANQUE EN PROCESOS DE LLENADO- CONTACTOR ENERGIZADO (BOMBA OPERANDO)");
    lampState = "Equipo en Marcha por Nivel E.Bajo-OFF  E.Alto-OFF  Bomba-ON";
          String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          textMessage = ""; 
    //digitalWrite(relay, HIGH);// Apaga el relé 
    flag = 0; //se habilita funcion TurnOn
    vacio = 1; //se desabilita bandera de vacio para enviar un sms
    lleno = 0;// se habilita bandera de lleno enviar sms
    bandera_llenadocompleto = 0;
    delay(500);
}

        DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
  
}


///////////// EB = 0;  EA = 1;  CONTACTOR = 0  TANQUE EN PROCESOS DE LLENADO- CONTACTOR ENERGIZADO ELECTRODO BAJO EN EL AGUA ////////////
void Tanque_Llenando()
{
   int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

   if( estado_EB == 0 && estado_EA == 1 && estado_contactor == 0)
   {
    lcd.setCursor(0,2);
    lcd.print("Pump On EA Off EB On");
    Serial.println("TANQUE EN PROCESOS DE LLENADO- CONTACTOR ENERGIZADO ELECTRODO BAJO EN EL AGUA");
   // digitalWrite(relay, HIGH);// Apaga el relé 
    delay(500);
   }
  
}


//////////////////////// Falla todo APAGADO EB = 1; EA = 1; CONTACTOR = 1 //////////////////
void FallaApagado()
{
  int estado_contactor = digitalRead(contactor);
  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

  if( estado_EB == 1 && estado_EA == 1 && estado_contactor == 1)
  {
    if (c2==1750)
    {
     lcd.setCursor(0,2);
    lcd.print("Pump Off EA Of EB Of");           
    Serial.println("ALERTAAAAAAAAAAA");
    lampState = " E.Bajo-OFF  E.Alto-OFF  Bomba-OFF - Ocurrio una falla equipo se encuentra apagado";
     String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
     //sendSMS(message);
     //sendSMS2(message);
     textMessage = ""; 
     c2=0;
    }
    c2=c2+1;
    
  }
    DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
  
}


//////////////////////////////Funcion envia mensaje equipo apagado por SMS se necesita activar sistema/////////////////

void Remoto_Off()
{

  int estado_EB = digitalRead(EB);
  int estado_EA = digitalRead(EA);

  if( estado_EB == 1 && estado_EA == 1 && (digitalRead (relay)==0))
       {

         // espera 750 equivalente a 13 minutos en caso de no re activar el sistema
          if(c==750)
          {
        lampState = "Equipo apagado previamente por SMS, Tanque por debajo del nivel del Electrodo Bajo, se necesita Activar el Sistema"; 
          String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          textMessage = "";
        c=0;
       }
       c=c+1;
       }

              DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
       
}

//funcion arranque de equipo
void TurnOn()
{
    if(textMessage.indexOf("ON")>=0)
    {
      digitalWrite(relay, HIGH);// Apaga el relé 
      delay(200);
  
       if (digitalRead(contactor)==1)
       {
          lampState = "Relevador de control ON, ALERTA, Bobina de Contactor desenergizada";
          String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          //reloj();
          Serial.println("Relay set to OFF");
          textMessage = ""; 
                    
             }

             
       else 
            {
              lampState = "Relevador de control ON, Equipo puesta en Marcha";
              String message =lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
              //sendSMS(message);
              //sendSMS2(message);
              //reloj();
               flag1 = 0; // se habilita el llenado
               flag2 = 0; // se habilita bandera Off  
               c=0; // se restablece el contador
              Serial.println("Relay set to ON");  
              textMessage = "";    
        }
    }
  

    //condicion equipo en arranque y contacto abierto "fallo equipo sobrecarga"
   /*  if (flag == 0) //bandera funcion TurnOn
  {
if (digitalRead (contactor)==1 && (digitalRead (relay)==1))
{
  if(c1==1800)
  {
   lampState = "Relevador de control OFF, ALERTA equipo con Falla"; 
    String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
    sendSMS(message);
    sendSMS2(message);
    //reloj();
    Serial.println("Relay set to OFF");
    textMessage = ""; 
    c1=0;
  }
  c1=c1+1;
}
  } // flag  */
      DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
 
}


//funcio paro de equipo
void TurnOff()
{
  
         if(textMessage.indexOf("OFF")>=0){
      digitalWrite(relay, LOW);// Enciende el relé 
      delay(200);
  
       if (digitalRead(contactor)==1){
          lampState = "Relevador de control OFF, Equipo Apagado"; 
          String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
          //sendSMS(message);
          //sendSMS2(message);
          //reloj();
          Serial.println("Relay set to OFF");
          flag1 = 1; // se desabilita llenado
          //flag = 0; // se habilita la funcion TurnOn
          flag2 =1; // se desabilita bandera funcion Off
          textMessage = ""; 
       }

       
       else 
       {
              lampState = "Relevador de control OFF - ALERTA, Bobina de Contactor quedo energizada "; 
              String message = lampState + " - Fecha: " + fecha1 + " - Hora:" + Hora1;
              //sendSMS(message);
              //sendSMS2(message);
              //reloj();
              Serial.println("Relay set to OFF");
              textMessage = "";   
        }
     }
         
  
     DateTime fecha = rtc.now();      // funcion que devuelve fecha y horario en formato
  // DateTime y asigna a variable 

   ano = fecha.year();
 mes = fecha.month();
 dia = fecha.day();
 Hr = fecha.hour();
 Min = fecha.minute();
 Seg = fecha.second();
 fecha1 = dia +"/" + mes + "/" + ano ;
 Hora1 =  Hr + ":" + Min + ":" + Seg ;
}





