# earnstej-heatpump-irsignal

Dette program bruges til at styre en varmepumpe i E Arnstej. Den er lavet til at køre på en Arduino Nano, men skulle også kunne køre på f.eks. Uno eller Pro Mini, samt diverse arduino-kloner.

Via IR kan en Panasonic NKE-model eller lignende styres. Dertil kan en TSAL6400 IR-diode eller tilsvarende bruges.

Krævet kodebibliotek:
* https://github.com/ToniA/arduino-heatpumpir

## Funktion

Varmepumpen kan enten være:

* Slukket
* Tændt på normal temperatur - slukker automatisk efter 24 timer eller indtil der aktivt slukkes

Inputs:

* Varmesignal - hvis kortvarigt lavt (forbundet til GND), skiftes mellem slukket og tændt. Indgangen trækkes op til 5 Volt via intern/ekstern pull up-modstand.
* Vertikal luftretning - hvis dette er lavt (0 Volt), køres lav luftretning, ellers høj luftretning. Indgangen trækkes op til 5 Volt via intern/ekstern pull up-modstand.

Outputs:

* IR-output - til at drive IR-diode som er koblet mellem digital udgang og GND via formodstand (se IR-diodens data for hvilken strøm der kræves - bemærk hvor mange milliampere der max. kan gives på udgangen).
* LED-status - lyser hvis varmen er tændt, eller indikerer aktivitet ved skift af indstilling eller opdatering af varmepumpe (IR-sending).
