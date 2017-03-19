#include "Guarder.h"
#include <EEPROM.h>

Guarder::Guarder(byte buzzerPin, byte modulesCount, Module modules[], unsigned long eepromStart) :
	buzzerPin(buzzerPin),
	modulesCount(modulesCount),
	modules(modules),
	eepromStart(eepromStart),
	lastUpdate(0) {

}

void Guarder::setup() {
	pinMode(buzzerPin, OUTPUT);

	for (byte i = 0; i < modulesCount; ++i) {
		auto module = modules[i];

		pinMode(module.guardPin, INPUT);
		pinMode(module.buttonPin, INPUT);
		pinMode(module.statusPin, OUTPUT);
		pinMode(module.warningPin, OUTPUT);
	}

	{ // Load state from EEPROM
		byte data = EEPROM.read(eepromStart);

		if ((data & 0x0F) == ((~data >> 4) & 0x0F)) { // Is our data valid?
			for (byte i = 0; i < modulesCount; ++i) {
				if (data & (1 << i)) {
					modules[i].status = Module::ModuleStatus::On;
				}
			}
		}
	}
}

void Guarder::loop() {
	unsigned long time = millis();
	bool buzzing = false;

	update = false;


	bool updateNow = false;

	if ((time - lastUpdate) > 100) {
		lastUpdate = time;
		updateNow = true;
	}

	for (byte i = 0; i < modulesCount; ++i) {
		auto &module = modules[i];

		auto buttonStatus = digitalRead(module.buttonPin) == HIGH;

		if (buttonStatus) {
			if (module.lastButtonClick == 0) {
				module.lastButtonClick = time;
			}
		} else {
			if (module.lastButtonClick != 0) {
				unsigned long clickTime = time - module.lastButtonClick;
				module.lastButtonClick = 0;

				if (clickTime > 50) {
					if (module.lastButtonUp == 0) {
						module.clickTime = clickTime;
						module.lastButtonUp = time;
					} else {
						onButtonClick(module, 0, true);
						module.lastButtonUp = 0;
					}
				}
			}

			if ((module.lastButtonUp != 0) && (time - module.lastButtonUp > 200)) {
				onButtonClick(module, module.clickTime, false);

				module.lastButtonUp = 0;
			}
		}


		if (module.status != Module::ModuleStatus::Off) {
			if (updateNow) {
				bool moduleStatus = digitalRead(module.guardPin) == HIGH;

				if ((module.status == Module::ModuleStatus::Delayed) ||
					(module.status == Module::ModuleStatus::Delayed2)) {
					if (module.lastWarningLight == 0) {
						module.lastWarningLight = time;

						digitalWrite(module.statusPin, LOW);
					} else if (time - module.lastWarningLight > 500) {
						module.warningLight = !module.warningLight;
						module.lastWarningLight = time;

						digitalWrite(module.statusPin, (module.warningLight) ? (HIGH) : (LOW));
					}

					if ((moduleStatus) && (module.status == Module::ModuleStatus::Delayed2)) {
						module.status = Module::ModuleStatus::On;
					} else if ((!moduleStatus) && (module.status == Module::ModuleStatus::Delayed)) {
						module.status = Module::ModuleStatus::Delayed2;
					}
				} else {
					digitalWrite(module.statusPin, HIGH);

					if (moduleStatus) { // Door closed
						module.lastWarningLight = 0;

						if (module.status == Module::ModuleStatus::WithoutWarning) {
							module.status = Module::ModuleStatus::On;
						}

						digitalWrite(module.warningPin, (module.wasOpened) ? (HIGH) : (LOW));
					} else { // Door opened
						if (module.lastWarningLight == 0) {
							module.lastWarningLight = time;

							digitalWrite(module.warningPin, LOW);
						} else if (time - module.lastWarningLight > 250) {
							module.warningLight = !module.warningLight;
							module.lastWarningLight = time;

							digitalWrite(module.warningPin, (module.warningLight) ? (HIGH) : (LOW));
						}

						if (module.status != Module::ModuleStatus::WithoutWarning) {
							module.status = Module::ModuleStatus::Warning;
						}

						module.wasOpened = true;
					}
				}
			}

			if (module.status == Module::ModuleStatus::Warning) {
				buzzing = true;
			}
		} else {
			digitalWrite(module.statusPin, LOW);
		}
	}

	if (buzzing) {
		if ((time - lastBuzz) > 50) {
			playBuzzer();

			lastBuzz = time;
		}
	} else {
		buzzPosition = 0;

		noTone(buzzerPin);
	}

	if (update) { // Save state to EEPROM
		byte data = 0;

		for (byte i = 0; i < modulesCount; ++i) {
			if (modules[i].status != Module::ModuleStatus::Off) {
				data |= (1 << i);
			}
		}

		data = data | ((~data) << 4);

		EEPROM.write(eepromStart, data);
	}
}

void Guarder::onButtonClick(Module &module, unsigned long clickTime, bool isDouble) {
	if (isDouble) {
		module.status = Module::ModuleStatus::Delayed;
	} else {
		if ((clickTime > 50) && (clickTime <= 500)) {
			if (module.status != Module::ModuleStatus::Off) {
				module.status = Module::ModuleStatus::WithoutWarning;
				module.wasOpened = false;
			}
		} else if ((clickTime > 500) && (clickTime <= 2500)) {
			if (module.status == Module::ModuleStatus::Off) {
				module.status = Module::ModuleStatus::On;
			} else {
				module.status = Module::ModuleStatus::Off;

				digitalWrite(module.statusPin, LOW);
				digitalWrite(module.warningPin, LOW);
				module.lastWarningLight = 0;
				module.warningLight = false;
				module.wasOpened = false;
				module.lastButtonClick = 0;
			}

			update = true;
		}
	}
}

void Guarder::playBuzzer() {
	char notes[] = "c2g2C2C2";
	int length = sizeof(notes) / sizeof(notes[0]) / 2;
	
	auto note = notes[buzzPosition * 2];
	auto beat = (int)(notes[buzzPosition * 2 + 1] - '0');

	char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
	int tones[] = { 1915, 1700, 1519, 1432, 1275, 1136, 1014, 956 };

	for (int i = 0; i < 8; i++) {
		if (names[i] == note) {
			tone(buzzerPin, tones[i], 1000 / beat);
			break;
		}
	}

	buzzPosition = (buzzPosition + 1) % length;
}