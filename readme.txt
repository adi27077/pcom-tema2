Musat-Burcea Adrian - 322CB - PC Tema 2

Structurile implementate peste protocoalele UDP/TCP
Am creat cate o structura pentru fiecare tip de mesaj ce poate fi transmis
intre server si clienti:
    - Mesaj de la clienti UDP la server contine:
        - topicul, sir de maxim 50 de caractere
        - tipul de date, uint pe 1 octet
        - payloadul, sir de 1500 de caractere + terminator
    - Mesaj de la clienti TCP la server (comanda) contine:
        - tipul comenzii, uint pe 1 octet (1 pt subscribe, 2 pt unsubscribe)
        - topicul, sir de 50 de caractere + terminator
        - sf, uint pe 1 octet (poate fi 0 sau 1)
    - Mesaj de la server la clienti TCP contine:
        - portul, uint pe 2 octeti
        - ip, sir de 16 caractere
        - tipul de date, sir de maxim 10 caractere + terminator (ex: SHORT_REAL)
        - payloadul, sir de 1500 de caractere + terminator
    Mesajul pt clientii TCP contine datele in format human-readable pentru a fi
    gata de afisat de catre client
Utilizand aceste structuri, ele se pot converti direct la bufferul care este
trimis prin retea si vice-versa

Functionarea serverului:
Pentru memorarea clientilor folosesc un hashmap cu chei ID si valoare client.
Structura client contine:
    - statutul de online/offline
    - socketul pe care clientul este conectat cand este online (-1 pt offline)
    - lista de topicuri la care este abonat (structura topic are la randul ei
        numele topicului si sf)
    - coada de mesaje care vor fi trimise clientului cand acesta se reconecteaza
Serverul initializeaza socketii pt conexiuni UDP/TCP si incepe ascultarea pt
conexiuni TCP. Serverul multiplexeaza intrarile intre citirea de la tastatura,
socketul UDP, socketul de ascultare pt conexiuni TCP si socketii clientilor TCP

In cazul inputului de la tastatura se verifica pt comanda exit. Daca este primita
se iese din bucla serverului, se inchid toti socketii deschisi si se opreste
serverul. Daca este comanda gresita, se afiseaza in stderr. 

In cazul primirii pe socketul UDP, se primeste mesajul de la clientul UDP,
se salveaza datele in structura de mesaj UDP, iar apoi se creaza mesajul TCP
folosind datele din mesajul UDP si urmarind cerintele din enunt. Mesajul TCP
contine datele intr-un format gata de afisat de catre clienti.
Dupa crearea mesajului TCP incepe procesul de trimitere al acestuia catre
clienti. Se itereaza prin mapul de clienti si pt fiecare client se verifica
daca este online sau nu. Daca da, se verifica daca clientul este abonat la
topicul mesaj (sf 1/0 nu conteaza) si daca da i se trimite mesajul. Daca
este offline clientul, se verifica daca este abonat la topicul mesajului cu
sf 1, iar daca da mesajul este adaugat in coada de mesaje a clientului.

In cazul primirii pe socketul de conexiuni TCP, se dezactiveaza algoritmul
Nagle si se primeste ID-ul noului client. Se verifica daca avem deja in hashmap
clientul cu ID-ul respectiv. Daca da, verificam daca el este online. Daca este
online, inseamna ca alt client vrea sa se conecteze cu ID-ul deja folosit si
refuzam si inchidem conexiunea. Daca este offline, inseamna ca el se
reconecteaza, deci se marcheaza din nou ca fiind online si i se atribuie
noul socket. In caza ca are mesaje in coada, acestea ii sunt trimise.
Daca clientul nu exista in hashmap, inseamna ca este prima data cand acesta
se conecteaza si il adaugam in hashmap. 

Ultimul caz, se primeste ceva de la un client TCP (comanda). Daca apelul
recv() intoarce 0 inseamna ca clientul s-a deconectat, de se marcheaza 
ca fiind offline in hashmap si se inchide socketul pe care acesta era conectat.
Daca se primeste o comanda verificam daca este de subscribe sau unsubscribe.
La subscribe, se adauga topicul primit in lista daca acesta nu exista si se
seteaza sf-ul lui. La unsubscribe, se elimina topicul din lista daca acesta
exista.

Inainte de inchiderea serverului, se inchid totii socketii existenti.

Functionarea clientului TCP (subscriber):
Se deschide socketul pentru conectarea la server si se dezactiveaza
algoritmul Nagle. Dupa conectare, se trimite ID-ul clientului la server.
Clientul multiplexeaza intrarile intre citirea de la tastatura si primirea
mesajelor de la server.

Cand se citeste de la tastatura se parseaza comanda. Daca avem exit, se inchide
clientul. La subscribe/unsubscribe se creaza un mesaj de tip comanda, ce contine
tipul comenzii, topicul si sf(doar pt subscribe). Dupa crearea mesajului, acesta
se trimite la server, iar daca este trimis cu succes, se afiseaza efectul de
catre client. Daca este gresita comanda, se afiseaza un mesaj de eroare in stderr.

In cazul in care se primeste mesaj de la server, acesta este pur si simplu
afisat, deoarece contine toate informatiile deja prelucrate, in format human
readable. 
