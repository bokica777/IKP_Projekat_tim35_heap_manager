Potrebno je implementirati Heap Manager koji podržava dve operacije:
● void *allocate_memory(int size)
● void free_memory(void* address)
Heap manager treba da slobodnu memoriju čuva organizovanu u segmente predefinisane veličine čiji
broj zavisi od potreba. Granica za oslobađanje nekorišćenih segmenata je 5 slobodnih segmenata. Heap
manager treba da omogući biranje između sledećih algoritama za traženje slobodnog prostora:
● next fit
Portebno je omogućiti rad u okruženju sa više niti (thread safety) kao ponuditi rešenje za izbegavanje
zagušenja u višenitnom okruženju.
Rešenje treba biti instrumentalizovano tako da je moguće pratiti:
1) Broj alokacija
2) Broj dealokacija
3) Stepen fragmentacije memorije
Biblioteku je potrebno uključiti u jednostavnu serversku aplikaciju koja jer koristi za alociranje i zahteva
od klijenta.