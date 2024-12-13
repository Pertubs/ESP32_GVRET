//================================================================INCLUDES
#include "config.h"
#include <esp32_can.h>
#include <SPI.h>
#include <Preferences.h>
#include "ELM327_Emulator.h"
#include "SerialConsole.h"
#include "wifi_manager.h"
#include "gvret_comm.h"
#include "can_manager.h"
#include "lawicel.h"


#define CAN_RX GPIO_NUM_19
#define CAN_TX GPIO_NUM_18


byte i = 0;
uint32_t lastFlushMicros = 0;
bool markToggle[6];
uint32_t lastMarkTrigger = 0;
uint8_t espChipRevision;
char deviceName[20];
char otaHost[40];
char otaFilename[100];
//================================================================OBJECTS
CAN_COMMON *canBuses[NUM_BUSES];

WiFiManager wifiManager;
GVRET_Comm_Handler serialGVRET; //gvret protocol over the serial to USB connection
GVRET_Comm_Handler wifiGVRET; //GVRET over the wifi telnet port
CANManager canManager; //keeps track of bus load and abstracts away some details of how things are done
SystemSettings SysSettings;
EEPROMSettings settings;
Preferences nvPrefs;
SerialConsole console;
LAWICELHandler lawicel;
ELM327Emu elmEmulator;
//================================================================FUNCTIONS
void handleCAN0CB(CAN_FRAME *frame)
{
	CAN0.sendFrame(*frame);
}

/*Send CanFrame over Serial
 message format
 1 = GVRET_Binary DEFAULT
 2 = GVRET_String 
 3 = Serial_Print_Frame
 */
void SendFrame(const CAN_FRAME &frame, int MessageFormat) 
{
    uint32_t timestamp = micros(); // Timestamp atual em microssegundos

    if (MessageFormat == 1)
    {
        // GVRET_Binary
        // Criação de buffer para envio
        uint8_t buffer[20];
        size_t index = 0;

        buffer[index++] = 0xF1; // Prefixo
        buffer[index++] = 0x00; // Comando para enviar frame

        // Timestamp (LSB to MSB)
        buffer[index++] = (timestamp >> 0) & 0xFF;
        buffer[index++] = (timestamp >> 8) & 0xFF;
        buffer[index++] = (timestamp >> 16) & 0xFF;
        buffer[index++] = (timestamp >> 24) & 0xFF;

        // ID do frame (LSB to MSB)
        uint32_t frameID = frame.id | (frame.extended ? 0x80000000 : 0x00000000);
        buffer[index++] = (frameID >> 0) & 0xFF;
        buffer[index++] = (frameID >> 8) & 0xFF;
        buffer[index++] = (frameID >> 16) & 0xFF;
        buffer[index++] = (frameID >> 24) & 0xFF;

        // Comprimento e número do bus
        uint8_t lengthAndBus = (frame.length & 0x0F) | ((0 & 0x0F) << 4); // Bus 0 por padrão
        buffer[index++] = lengthAndBus;

        // Dados (0–8 bytes)
        for (int i = 0; i < frame.length; i++) 
        {
            buffer[index++] = frame.data.uint8[i];
        }

        // Preencher com zeros se necessário
        for (int i = frame.length; i < 8; i++) 
        {
            buffer[index++] = 0x00;
        }

        buffer[index++] = 0x00; // Byte final

        // Enviar via Serial
        Serial.write(buffer, index);

        // Enviar via WiFi
        wifiManager.sendDataToClients(buffer, index); // Uso de write para envio WiFi
    }
    else if (MessageFormat == 2)
    {
        // GVRET_String
        String message;

        // Timestamp em microssegundos
        message += "T:";
        message += String(micros());
        message += " ";

        // ID do frame
        message += "ID:";
        message += String(frame.id, HEX);
        message += " ";

        // Extensão (standard ou extended)
        message += "EXT:";
        message += frame.extended ? "Y" : "N";
        message += " ";

        // Comprimento do frame
        message += "LEN:";
        message += String(frame.length);
        message += " ";

        // Dados (0–8 bytes)
        message += "DATA:";
        for (int i = 0; i < frame.length; i++) 
        {
            if (i > 0) 
            {
                message += " ";
            }
            message += String(frame.data.uint8[i], HEX);
        }

        // Final da mensagem
        message += "\n";

        // Enviar via Serial
        Serial.print(message);

        // Enviar via WiFi
        wifiManager.sendDataToClients((uint8_t*)message.c_str(), message.length()); // Conversão para uint8_t* para envio via WiFi
    }
    else if (MessageFormat == 3)
    {
        // Serial_Print_Frame
        Serial.print("ID: ");
        Serial.println(frame.id, HEX);
        Serial.print("Ext: ");
        Serial.println(frame.extended ? "Y" : "N");
        Serial.print("Len: ");
        Serial.println(frame.length, DEC);
        for (int i = 0; i < frame.length; i++) {
            Serial.print(frame.data.uint8[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    else 
    {
        Serial.println("Formato de mensagem desconhecido.");
    }  
}

void Init_CAN()
{ 


    Serial.println("INICIADO COMO WIFI");
    
     for (int i = 0; i < NUM_BUSES; i++) canBuses[i] = nullptr;

    nvPrefs.begin(PREF_NAME, false);

    //settings.wifiMode == 1 ? strcpy(settings.SSID, "simova-engenharia") : strcpy(settings.SSID, "Sniffer");+

    strcpy(settings.SSID, "simova-engenharia");
    strcpy(settings.WPA2Key, "1234engenharia");

    settings.useBinarySerialComm = nvPrefs.getBool("binarycomm", false);
    settings.logLevel = nvPrefs.getUChar("loglevel", 1); //info
    settings.wifiMode = nvPrefs.getUChar("wifiMode", 2); //Wifi defaults to creating an AP
    settings.enableBT = nvPrefs.getBool("enable-bt", false);
    settings.enableLawicel = nvPrefs.getBool("enableLawicel", true);
    settings.systemType = nvPrefs.getUChar("systype", (espChipRevision > 2) ? 0 : 1); //0 = A0, 1 = EVTV ESP32

        canBuses[0] = &CAN0;
        SysSettings.logToggle = false;
        SysSettings.txToggle = true;
        SysSettings.rxToggle = true;
        SysSettings.lawicelAutoPoll = false;
        SysSettings.lawicelMode = false;
        SysSettings.lawicellExtendedMode = false;
        SysSettings.lawicelTimestamping = false;
        SysSettings.numBuses = 1; //Currently we support CAN0
        SysSettings.isWifiActive = false;
        SysSettings.isWifiConnected = false;

        strcpy(deviceName, MACC_NAME);
        strcpy(otaHost, "macchina.cc");
        strcpy(otaFilename, "/a0/files/a0ret.bin");


	CAN0.setCANPins(CAN_RX, CAN_TX);
    canManager.SimpleSetup(250000);
	CAN0.setRXFilter(0, 0x100, 0x700, false);
	CAN0.watchFor(); //allow everything else through
	CAN0.setCallback(0, handleCAN0CB);
    
    wifiManager.setup();

}

void Read_CAN()
 {
	CAN_FRAME message;
	if (CAN0.read(message)) 
    {
        SendFrame(message, 1);
	}
}

 void Init_Serial()
{
    Serial.begin(1000000); //BAUDRATE FOR SAVVYCAN
    //Serial.begin(115200); //BAUDRATE FOR SERIAL

    Serial.flush();
    Serial.setTimeout(10);
    Handshake();
}

void Update_GVRET()
{
    int serialCnt;
    uint8_t in_byte;
    if (SysSettings.lawicelPollCounter > 0) SysSettings.lawicelPollCounter--;
    //}

    canManager.loop();
    wifiManager.loop();

    size_t wifiLength = wifiGVRET.numAvailableBytes();
    size_t serialLength = serialGVRET.numAvailableBytes();
    size_t maxLength = (wifiLength>serialLength) ? wifiLength : serialLength;

    //If the max time has passed or the buffer is almost filled then send buffered data out
    if ((micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL) || (maxLength > (WIFI_BUFF_SIZE - 40)) ) 
    {
        lastFlushMicros = micros();
        if (serialLength > 0) 
        {
            Serial.write(serialGVRET.getBufferedBytes(), serialLength);
            serialGVRET.clearBufferedBytes();
        }
        if (wifiLength > 0)
        {
            wifiManager.sendBufferedData();
        }
    }

    serialCnt = 0;
    while ( (Serial.available() > 0) && serialCnt < 128 ) 
    {
        serialCnt++;
        in_byte = Serial.read();
        serialGVRET.processIncomingByte(in_byte);
    }

}

void Update_Watchdog()
{
    vTaskDelay(1/portTICK_PERIOD_MS);
}

void HandleHandshake(Stream &serialPort)
 {
    uint8_t receivedData[64]; // Buffer para dados recebidos
    uint8_t response[64];     // Buffer para resposta
    size_t receivedLength = 0;

    // Processa enquanto houver dados no buffer serial
    while (serialPort.available() > 0) {
        // Lê dados recebidos
        receivedLength = serialPort.readBytes(receivedData, sizeof(receivedData));

        // Processa comandos apenas se o prefixo for 0xF1
        if (receivedLength > 0 && receivedData[0] == 0xF1) {
            uint8_t command = receivedData[1]; // Comando recebido

            // Determina a resposta com base no comando
            switch (command) {
                case 0x07: // Get Device Info
                    response[0] = 0xF1;   // Prefixo
                    response[1] = 0x07;   // Comando
                    response[2] = 0x01;   // Versão do firmware LSB
                    response[3] = 0x00;   // Versão do firmware MSB
                    response[4] = 0x01;   // Versão da EEPROM
                    response[5] = 0x01;   // Tipo de saída
                    serialPort.write(response, 6);
                    break;

                case 0x09: // Keep Alive
                    response[0] = 0xDE;   // Resposta Keep Alive
                    response[1] = 0xAD;
                    serialPort.write(response, 2);
                    break;

                case 0x0C: // Set Baud Rate (placeholder)
                    response[0] = 0xF1;   // Prefixo
                    response[1] = 0x0C;   // Echo do comando
                    serialPort.write(response, 2);
                    break;

                case 0x0A: // Handshake Confirmado
                    response[0] = 0xF1;   // Prefixo
                    response[1] = 0x0A;   // Comando Handshake
                    serialPort.write(response, 2);
                    break;

                default:
                    // Comando desconhecido, ignorar ou logar
                    break;
            }
        }
    }
}

void Handshake()
{
    Serial.println("GVRET-PC");
    // Serial.write(0xE7); // Ativa o modo binário    
    // Serial.write(0xF1); // Prefixo
    // Serial.write(0x09); // Keep Alive
    // Serial.write(0xDE); // Resposta esperada
    // Serial.write(0xAD);
    HandleHandshake(Serial);

}

void setup()
{
    Init_Serial();
    Init_CAN();
}

void loop()
{

   Read_CAN();
   Update_GVRET();
   Update_Watchdog();

}