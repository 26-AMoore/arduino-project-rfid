//{{{ includes and macros
#include "HardwareSerial.h"
#include "SPI.h"
#include <Arduino.h>
#include <MFRC522.h>
#include <Hash.h>
#define RST_PIN         5
#define SS_PIN          53
#define ANNOY_LED       3
#define READER_LED      2
#define s_print(str) (Serial.print(str))
//}}}

// enums {{{
enum Annoy_Levels {
	VERY,
	LITTLE,
	SILENT
};

Annoy_Levels annoyLevel = VERY;

enum Mode {
	READING,
	OPENED,
	WRITING,
};
//}}}

struct /*{{{*/ Query {
	uint8_t size = 18;
	byte* data = (byte*) malloc(size);
} query; //}}}

Mode mode = READING;

#define TABLE_LEN       10
#define DIGEST_LEN      20

void /*{{{*/ dump_byte_array(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
		s_print(buffer[i] < 0x10 ? " 0" : " ");
		Serial.print(buffer[i], HEX);
	}
} //}}}

struct /*{{{*/ Hashtable { //not a hashmap
	unsigned short enabled = 65535; //used as a bitmap of which hashes are enabled
	byte digestlen = 20;
	byte hash[TABLE_LEN * DIGEST_LEN] = {
	0x5B, 0xA9, 0x3C, 0x9D, 0xB0, 0xCF, 0xF9, 0x3F, 0x52, 0xB5, 0x21, 0xD7, 0x42, 0x0E, 0x43, 0xF6, 0xED, 0xA2, 0x78, 0x4F,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

} /*}}}*/ hashtable;
//bad bad bad
bool /*{{{*/ validate(Hashtable table, byte hash[]) {
	Serial.println("validating card");
	bool valid = false;
	short validBytes = 0;

	for (short i = 0; i < TABLE_LEN; i++) {
		if (((2^i) & table.enabled) > 0) {
			Serial.println("istg");
			for (short j = 0; j < table.digestlen; j++) {
				Serial.println("checking");
				if (table.hash[i * table.digestlen + j] == hash[j]) {
					validBytes++;
					Serial.println(validBytes);
				}
				if (validBytes == 20) {
					s_print("VALID");
					valid = true; //mark
				}
			}
		}
		validBytes = 0;
	}
	return valid;
} //}}}

MFRC522 rfid = MFRC522(SS_PIN, RST_PIN );

MFRC522::MIFARE_Key key;

Mode /*{{{*/ checkCard() {
	Serial.println("checking card");
	Serial.print("	");
	if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

		rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 0, &key, &rfid.uid);
		
		for (int i = 0; i < 4; i++) {
			rfid.MIFARE_Read(i, query.data, &query.size);
			dump_byte_array(query.data, query.size);
			Serial.println();
			byte hash[20];
			sha1(query.data, query.size, &hash[0]);

			if (validate(hashtable, &hash[0])) {
				Serial.println("valid somehow?");
				rfid.PCD_StopCrypto1();
				annoyLevel = VERY;
				return Mode::OPENED;

			}
		}

		rfid.PCD_StopCrypto1();
		return Mode::READING;
	}
	return Mode::READING;
} //}}}

int prevTime = 0;
int elapsedTime = 0;
void /*{{{*/ blink(int led, int bri) {
	elapsedTime = millis();
	if (elapsedTime - prevTime > 500) {
		analogWrite(led, 0);
	} else {
		analogWrite(led, bri);
	}

	if (elapsedTime - prevTime > 1000) {
		prevTime = elapsedTime;
	}
} //}}};


void /*{{{*/ annoy(Annoy_Levels annoy) {
	switch (annoy) {
		case VERY:
			blink(ANNOY_LED, 255);
			break;
		case LITTLE:
			blink(ANNOY_LED, 10);
			break;
		case SILENT:
			break;
	}
} //}}}

void setup() {
	Serial.begin(9600);

	pinMode(ANNOY_LED, OUTPUT);
	SPI.begin();
	rfid.PCD_Init();

	for (int i = 0; i < 6; i++) {
		key.keyByte[i] = 0xff;
	}

}

void loop() {
	switch (mode) {
		case READING:
			Serial.println("READING");
			Serial.print("	");
			mode = checkCard();
			break;
		case OPENED:
			Serial.println("OPEN");
			annoy(annoyLevel);
			break;
		case WRITING:
			Serial.println("LITERALLY HOW");
			break;
	}
}
