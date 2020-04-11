# earnstej-heatpump-irsignal

Dette program bruges til at styre en varmepumpe i E Arnstej. Den er lavet til at køre på en Arduino Nano, men skulle også kunne køre på f.eks. Uno eller Pro Mini, samt diverse arduino-kloner.

Via IR kan en Panasonic NKE-model eller lignende styres. Dertil kan en TSAL6400 IR-diode eller tilsvarende bruges.

Krævede kodebiblioteker:
* https://github.com/ToniA/arduino-heatpumpir

## Funktion

Varmepumpen kan enten være:

* Slukket
* På sænket temperatur (8/10 grader, fuld ventilation - en indbygget funktion i varmepumpen).
* Tændt på 25 grader

Inputs:

* Varmesignal, 3 stk. - hvis et af dem er lavt (0 Volt), tændes varmepumpen. Svævende indgange betragtes som høje (5 Volt via interne pull up-modstande). Varmesignaler kan f.eks. være:
  * Tænd-knap som brugere kan aktivere for at få varme tændt.
  * Automatisk signal, f.eks. fra timer der aktiverer et relæ som kobler indgang til Vcc (5 Volt).
* Frost-signal - hvis dette er lavt (0 Volt), køres sænket temperature i stedet for at slukke varmepumpen når der ikke er varmesignal. Svævende indgang betragtes som høj (5 Volt via intern pull up-modstand). Frost-signal kan f.eks. være:
  * Knap som stilles til mildt vejr eller frostvejr.
* Opdateringssignal - hvis dette momentant - dvs. i 1 sekund - er lavt (0 Volt), sendes et ekstra IR-signal til varmepumpen (kan evt. bruges hvis IR-pumpen er blevet påvirket med fjernbetjening eller af anden årsag kører anderledes end forventet). Svævende indgang betragtes som høj (5 Volt via intern pull up-modstand). Dette signal bruges ikke nødvendigvis.

Outputs:

* IR-output - til at drive IR-diode som er koblet mellem digital udgang og GND via formodstand (se IR-diodens data for hvilken strøm der kræves - bemærk hvor mange milliampere der max. kan gives på udgangen).
