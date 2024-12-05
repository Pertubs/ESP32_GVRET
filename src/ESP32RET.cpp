//================================================================INCLUDES
#include "config.h"
#include <esp32_can.h>
#include <SPI.h>
#include <Preferences.h>
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

//================================================================OBJECTS
CAN_COMMON *canBuses[NUM_BUSES];

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

    if(MessageFormat == 1)
    {
                 // GVRET_Binary
            // Enviar início da mensagem binária
            Serial.write(0xF1); // Prefixo
            Serial.write(0x00); // Comando para enviar frame

            // Enviar timestamp (LSB to MSB)
            Serial.write((timestamp >> 0) & 0xFF);
            Serial.write((timestamp >> 8) & 0xFF);
            Serial.write((timestamp >> 16) & 0xFF);
            Serial.write((timestamp >> 24) & 0xFF);

            // Enviar ID do frame (LSB to MSB, inclui flag de extended frame)
            uint32_t frameID = frame.id | (frame.extended ? 0x80000000 : 0x00000000);
            Serial.write((frameID >> 0) & 0xFF);
            Serial.write((frameID >> 8) & 0xFF);
            Serial.write((frameID >> 16) & 0xFF);
            Serial.write((frameID >> 24) & 0xFF);

            // Enviar comprimento e número do bus
            uint8_t lengthAndBus = (frame.length & 0x0F) | ((0 & 0x0F) << 4); // Bus 0 por padrão
            Serial.write(lengthAndBus);

            // Enviar dados (0–8 bytes)
            for (int i = 0; i < frame.length; i++) 
            {
                Serial.write(frame.data.uint8[i]);
            }

            // Preencher com zeros se necessário
            for (int i = frame.length; i < 8; i++) 
            {
                Serial.write(0x00);
            }

            // Enviar byte final
            Serial.write(0x00); // Indica término da mensagem
    }
    else if(MessageFormat == 2)
    {
        // GVRET_String
            // Enviar início da mensagem binária
            Serial.write(0xF1); // Prefixo
            Serial.write(0x00); // Comando para enviar frame

            // Enviar timestamp (LSB to MSB)
            Serial.write((timestamp >> 0) & 0xFF);
            Serial.write((timestamp >> 8) & 0xFF);
            Serial.write((timestamp >> 16) & 0xFF);
            Serial.write((timestamp >> 24) & 0xFF);

            // Enviar ID do frame (LSB to MSB, inclui flag de extended frame)
            uint32_t frameID = frame.id | (frame.extended ? 0x80000000 : 0x00000000);
            Serial.write((frameID >> 0) & 0xFF);
            Serial.write((frameID >> 8) & 0xFF);
            Serial.write((frameID >> 16) & 0xFF);
            Serial.write((frameID >> 24) & 0xFF);

            // Enviar comprimento e número do bus
            uint8_t lengthAndBus = (frame.length & 0x0F) | ((0 & 0x0F) << 4); // Bus 0 por padrão
            Serial.write(lengthAndBus);

            // Enviar dados (0–8 bytes)
            for (int i = 0; i < frame.length; i++) 
            {
                Serial.write(frame.data.uint8[i]);
            }

            // Preencher com zeros se necessário
            for (int i = frame.length; i < 8; i++) 
            {
                Serial.write(0x00);
            }

            // Enviar byte final
            Serial.write(0x00); // Indica término da mensagem
    }
    else if(MessageFormat == 3)
    {
            // Serial_Print_Frame
            // Print message
            Serial.print("ID: ");
            Serial.println(frame.id, HEX);
            Serial.print("Ext: ");
            if (frame.extended) {
                Serial.println("Y");
            } else {
                Serial.println("N");
            }
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
	CAN0.setCANPins(CAN_RX, CAN_TX);
	if(CAN0.begin(250000))
	{
		Serial.println("Builtin CAN Init OK ...");
	} else {
		Serial.println("BuiltIn CAN Init Failed ...");
	}
	
	CAN0.setRXFilter(0, 0x100, 0x700, false);
	CAN0.watchFor(); //allow everything else through
	CAN0.setCallback(0, handleCAN0CB);
    
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

void Update_Watchdog()
{
    vTaskDelay(1/portTICK_PERIOD_MS);
}

void Handshake()
{
    Serial.println("GVRET-PC");
    Serial.write(0xE7); // Ativa o modo binário    
}

void setup()
{
    Init_Serial();
    Init_CAN();
}

void loop()
{
   Read_CAN();
   Update_Watchdog();

}