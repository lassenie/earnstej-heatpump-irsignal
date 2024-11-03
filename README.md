# earnstej-heatpump-irsignal

Dette program bruges til at styre en varmepumpe i E Arnstej. Den er lavet til at køre på en Arduino Nano, men skulle også kunne køre på f.eks. Uno eller Pro Mini, samt diverse arduino-kloner.

Via IR kan en Panasonic NKE-model eller lignende styres. Dertil kan en TSAL6400 IR-diode eller tilsvarende bruges.

Krævede kodebiblioteker:
* https://github.com/ToniA/arduino-heatpumpir

## Funktion

Varmepumpen kan enten være:

* Slukket
* Tændt på normal temperatur (se temperaturvalg nedenfor) - slukker automatisk efter 24 timer

Inputs:

* Varmesignal, 3 stk. - hvis et af dem er lavt (0 Volt), skiftes mellem slukket og tændt. Svævende indgange betragtes som høje (5 Volt via interne pull up-modstande). De tre indgange giver:
  * 12 timer (nok til et møde)
  * 24 timer (nok til et dags-/døgnarrangement)
  * 72 timer (nok til en weekend)
* Vertikal luftretning - hvis dette er lavt (0 Volt), køres høj luftretning, ellers lav luftretning.
* Opdateringssignal - hvis dette momentant er lavt (0 Volt), sendes et ekstra IR-signal til varmepumpen (kan evt. bruges hvis IR-pumpen er blevet påvirket med fjernbetjening eller af anden årsag kører anderledes end forventet). Svævende indgang betragtes som høj (5 Volt via intern pull up-modstand). Dette signal bruges ikke nødvendigvis.
* Temperaturvalg, 2 stk. vælger 22, 23, 24 eller 25 grader afhængigt af hvilke af de to inputs der er forbundet til temperaturvalgs-udgangen.

Outputs:

* IR-output - til at drive IR-diode som er koblet mellem digital udgang og GND via formodstand (se IR-diodens data for hvilken strøm der kræves - bemærk hvor mange milliampere der max. kan gives på udgangen).
* LED-status - til at indikere aktivitet ved skift af indstilling eller opdatering af varmepumpe (IR-sending).
* Temperaturvalg - konstant lavt (0 Volt) til at trække evt. forbundne temperaturvalgs-indgange ned.
