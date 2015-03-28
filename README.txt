This was my third homework for a computer graphics course in the 3rd year.

Task: implementing a 3D scene in C++ using OpenGL to model the jumps of some
characters on two springboards (a simplified version of ski jumping). This
assignment had a rather educational purpose as the requirements were very
unrealistic (the arrow keys had to control the character speed instead of
acceleration, the character trajectory during jumps had to be a semicircle
followed by a straight line instead of a parabola and it wasn't too clear how
collisions should work). I didn't implement many additional features for this
homework as I had already reached the maximum number of bonus points allowed by
the course rules because of the first two homeworks.

Full statement:
http://cs.curs.pub.ro/2014/pluginfile.php/10228/mod_resource/content/2/Tema%203%20EGC_2014.pdf
(You might not have access to it as a guest.)

Check out the repo wiki for some screenshots.

Original README file:
================================================================================
EGC - Tema 3
    Sarituri de pe trambulina
    Mirea Stefan-Gabriel, 331CC

Cuprins

1. Cerinta
2. Utilizare
3. Implementare
4. Testare
5. Continutul Arhivei
6. Functionalitati

1. Cerinta
--------------------------------------------------------------------------------
Tema presupune implementarea unei scene 3D pentru a modela sariturile unor
personaje de pe doua trambuline, la baza carora se afla un quad pe care
personajele isi pot continua deplasarea. Se disting doua categorii de personaje,
principale si secundare, corespunzatoare trambulinelor. Din fiecare categorie,
un singur personaj este activ la un momentdat, avand rolul de a efectua o
saritura, in timp ce celelalte sunt inactive si il urmaresc pe cel activ.

2. Utilizare
--------------------------------------------------------------------------------
Singura interfata a programului o reprezinta inputul de la tastatura.

Functionalitati din enunt:
- 'A': Personajul principal activ devine inactiv, daca se afla pe quad;
- 'C': Activeaza urmatoarea camera, in ordinea din enunt (pentru camera
       anterioara, folositi Shift + 'C');
- 'E': Activeaza cutremurul, daca personajul principal activ se afla pe rampa;
- 'R': Dezactiveaza cutremurul;
- 'S': Deseneaza scena in modul solid;
- 'W': Deseneaza scena in modul wireframe;
- Sageti: Deplaseaza personajul principal activ, conform restrictiilor cerintei.

Nota: In timp ce camera FPS de sus este activa, aceasta va acapara semnificatia
unor taste. Pentru a pastra semnificatia din enunt, folositi litere mari
(combinatii cu Shift), care vor functiona indiferent de camera care este activa.

Controlul camerei FPS:
- 'w': Translatie inainte;
- 'a': Translatie la stanga;
- 's': Translatie inapoi;
- 'd': Translatie la dreapta;
- 'r': Translatie in sus;
- 'f': Translatie in jos;
- 'q': Rotatie la stanga (fata de Oy);
- 'e': Rotatie la dreapta (fata de Oy);
- 'z': Rotatie in sens contrar acelor de ceas (fata de Oz);
- 'x': Rotatie in sensul acelor de ceas (fata de Oz);
- 't': Rotatie in sus (fata de Ox);
- 'g': Rotatie in jos (fata de Ox);
- 'o': Reseteaza pozitia camerei.

Avand in vedere ca, pe masura rularii programului, in scena vor fi create tot
mai multe camere, comutarea mai simpla intre camerele importante (toate cu
exceptia celor statice, ale personajelor inactive) se poate realiza astfel:
- '1': Camera FPS a personajului principal activ;
- '2': Camera TPS a personajului principal activ;
- '3': Camera FPS a personajului secundar activ;
- '4': Camera TPS a personajului secundar activ;
- '5': Camera FPS de sus.

La acestea, se adauga functionalitatile din laboratorul 6:
- Esc: Inchide glut;
- Space: Reincarca shaderul si recalculeaza locatiile.

3. Implementare
--------------------------------------------------------------------------------
Platforma:
   IDE: Microsoft Visual Studio Professional 2013
   Compilator: Microsoft C/C++ Optimizing Compiler Version 18.00.21005.1 for x86
   Sistem de operare: Microsoft Windows 7 Ultimate SP1 Version 6.1.7601
   Placa video: ATI Mobility Radeon HD 4570 512MB VRAM

Generalitati:
Pentru toate obiectele din cadrul scenei am folosit un singur vector de vertecsi
si unul singur de indecsi, declarate ca si campuri private ale clasei Tema. Toate
metodele ce construiesc geometrie adauga elemente la aceste structuri, fie ca se
bazeaza pe apeluri ale metodelor add3Dobject(), loadSecGeometry() sau fac acest
lucru in mod particular (ca la construirea quadului). Pentru a face distinctia
intre obiecte diferite, vectorul obj_offset retine, pentru fiecare obiect,
pozitia din vertices de unde incep varfurile asociate. In corespondenta cu
obj_offset, vectorul model_matrices retine pentru fiecare obiect matricea de
modelare. Toate acestea sunt trimise catre vertex shader unde, pe baza
gl_VertexID, se fac cautari binare in vectorul obj_offset pentru identificarea
obiectului din care face parte vertexul curent si, implicit, a matricei de
modelare.
Pentru a tine evidenta tuturor tastelor apasate la un momentdat, folosesc doua
bitseturi:
  - pressed_keys retine pentru fiecare tasta 1 daca este apasata sau 0 in caz
    contrar;
  - is_special retine pentru fiecare tasta (daca e apasata) 1 daca e o tasta
    speciala sau 0 in caz contrar.
Astfel, multe din prelucrarile corespunzatoare tastelor sunt efectuate in
notifyBeginFrame(), verificand daca tastele sunt apasate in acel moment prin
apeluri ale metodei isPressed(). Rezultatul este o miscare mai cursiva, deoarece
nu mai apare delay-ul care cere confirmarea mentinerii tastelor apasate. Prin
urmare, efectele vor depinde cu precizie de timpul apasarii.
Toate camerele sunt retinute in lista inlantuita cams, in aceeasi ordine in care
sunt descrise in enunt, iar accesul la camerele importante nestatice, precum si
la locurile de insertie ale camerelor statice urmatoare se face pe baza mai
multor iteratori.
Pentru implementarea coliziunilor dintre doua paralelipipede ce nu pot avea
rotatie decat fata de Oy (intrucat coliziunile se verifica doar intre obiecte de
pe quad sau care se afla in cursul actiunii de cadere, cand li s-a corectat deja
rotatia), verific in primul rand in ce masura se suprapun intervalele pe Oy.
Daca aceste intervale se suprapun, verific suprapunerea bazelor (dreptunghiuri),
punand conditia ca cel putin un varf sau centrul unui dreptunghi sa se afle in
interiorul celuilalt (vezi sectiunea Testare).
Miscarea personajelor secundare active pe quad consta din rotiri si translatii
aleatoare, generate in metoda randomMove(). Acestea devin inactive la orice
coliziune cu peretii quadului sau cu alte personaje.

Mentiuni:
Actiunea de jump e realizata de-a lungul unui arc de cerc de ~140 grade (nu un
semicerc). Intrucat nici in imaginea din enunt nu era reprezentat un semicerc,
presupun ca acest lucru nu e prea important.
In timpul actiunii de jump se realizeaza si anularea rotirilor pe Ox si Oz: la
fiecare moment de timp, in functie de progresul determinat de unghiul subintins,
are loc mai intai anularea rotatiei fata de Oy, apoi rotirea fata de Oz cu un
unghi intermediar corespunzator progresului, apoi refacerea rotirii fata de Oy.
Doar culoarea personajului principal activ se calculeaza in vertex shader,
pentru restul personajelor principale o modific permanent in vectorul vertices.
Din acest motiv, constantele corespunzatoare celor doua culori sunt definite
atat in main.cpp cat si in shader_vertex.glsl.
Pentru ca in timpul cutremurelor, in cazul in care camera activa este cea FPS a
personajului principal activ, camera (ce sta pe loc) sa nu atinga personajul (ce
se cutremura), am trimis catre vertex shader o variabila (main_fps) care
precizeaza daca este activa aceasta camera, ceea ce va duce la anularea
cutremurarii personajului in caz afirmativ.
In timpul sariturii si caderii personajului principal activ, se respecta
directia pe care acesta o avea la parasirea rampei.

4. Testare
--------------------------------------------------------------------------------
Tema a fost testata doar pe calculatorul personal (platforma mentionata la 3.).
Am incercat sa aplic cam toate scenariile posibile si am corectat toate
problemele care au aparut, cu urmatoarele exceptii:

Vertex shader-ul nu accepta declararea de vectori fara sa se specifice
dimensiunea. Din acest motiv, am ales o constanta (MAX_OBJ_NUM, 500) pentru
numarul maxim de obiecte, ceea ce poate duce la probleme in cazul depasirii
acestei valori, pe masura ce noi personaje sunt create.

De asemenea, datorita modului in care verific suprapunerea a doua
paralelipipede, unele coliziuni ar putea sa nu fie detectate, si anume cand
proiectiile pe quad ale bazelor nu au niciun varf si nici centrul in proiectia
bazei celuilalt paralelipiped. Cum personajele principale sunt cuburi, acest
lucru s-ar putea intampla de fapt numai daca in locul unde cade un personaj
secundar activ ar exista deja un personaj secundar inactiv suficient de
translatat pe Ox si Oz si de rotit fata de Oy. Intrucat o astfel de situatie
s-ar intampla foarte rar, nu am testat acest caz. Cu toate astea, in enunt se
cereau coliziuni doar intre personajele aflate pe quad.

5. Continutul Arhivei
--------------------------------------------------------------------------------
resurse\dragon.obj - grafica personajelor secundare (fisierul din laboratorul 6)
shadere\shader_fragment.glsl - fragment shader-ul
shadere\shader_vertex.glsl - vertex shader-ul
lab_camera.hpp - modificat pentru implementarea camerei
lab_mesh_loader.hpp - incarcarea unui mesh din fisier - modificat pentru ca
                      functia loadObj() sa intoarca prin parametri vertecsii si
                      indecsii
main.cpp - sursa principala
Toate celelalte fisiere fac parte din scheletul din laborator.

6. Functionalitati
--------------------------------------------------------------------------------
6.1 Functionalitati standard (toate cele din enunt)

    - desenarea obiectelor scenei;
    - deplasarea personajului principal activ in scena;
    - deplasarea personajului secundar activ in scena;
    - camere;
    - efect de cutremur + culoarea personajului principal activ.

6.2 Functionalitati Bonus / Suplimentare

    - Personajele principale / secundare urmaresc evoluatia personajului
      principal / secundar din punctul cel mai inalt al rampei pana la atingerea
      quadului.
    - Rampele celor doua trambuline contin de fiecare parte parapete. In cazul
      personajului principal, sunt implementate si coliziunile cu acestea.
    - Quadul contine si el parapete de jur imprejur, fiind implementate
      coliziunile ambelor personaje active cu acestea.