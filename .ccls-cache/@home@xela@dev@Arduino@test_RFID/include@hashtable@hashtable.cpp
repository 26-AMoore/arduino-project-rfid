#ifndef HASHTABLE_H
#define HASHTABLE_H
#include "hashtable.h"
#include "Arduino.h"

#define s_print(str) (Serial.print(str))
#define TABLE_LEN       10
#define DIGEST_LEN      20


Hashtable hashtable;
//bad bad bad
bool validate(Hashtable table, byte hash[]) {
	bool valid = false;
	
	for (short i = 0; i < TABLE_LEN; i++) {
		if (((2^i) & table.enabled) > 0) {
			for (short j = 0; j < table.digestlen; i++) {
				if (table.hash[i * table.digestlen + j] == hash[j]) {
					s_print(hash[j]);
				}
			}
		}
	}

	return valid;
}
#endif
