# file_storage
*Progetto di laboratorio di sistemi operativi*

Il programma server è contenuto nella cartella *server*.
Per compilarlo eseguire il comando **make** mentre ci si trova in essa.
Dopo la compilazione l'eseguibile *fsp_server* si troverà nella medesima cartella.
Per rimuovere tutti i file generati durante la compilazione
eseguire il comando **make clean**.

Il programma client è contenuto nella cartella *client*.
Per compilarlo eseguire il comando **make** mentre ci si trova in essa.
Dopo la compilazione l'eseguibile *fsp* si troverà nella medesima cartella.
Per rimuovere tutti i file generati durante la compilazione
eseguire il comando **make clean**.

È possibile compilare sia il server che il client eseguendo
il comando **make all** mentre ci si trova nella cartella root dell'attuale repository.
In tal caso verranno create anche le cartelle necessarie per eseguire successivamente i test.
Per rimuovere tutti i file generati eseguire il comando **make cleanall**.

Nella cartella *tests* sono presenti i tre eseguibili *test1.sh*, *test2.sh* e *test3.sh*.
Inoltre è presente anche la cartella *files* contenente alcuni file per eseguire i test.
Trovandosi nella cartella root dell'attuale repository è possibile eseguire i tre test
con i seguenti comandi:

    make all
    make test1
    // Leggere gli output generati dal server e dal client nelle relative cartelle contenute in tests
    make cleanall

    make all
    make test2
    // Leggere gli output generati dal server e dal client nelle relative cartelle contenute in tests
    make cleanall

    make all
    make test3
    // Leggere gli output generati dal server e dal client nelle relative cartelle contenute in tests
    make cleanall

Inoltre, dopo aver eseguito il programma server (anche dopo uno dei test) è possibile produrre
sullo standard output un sunto delle operazioni realizzate dal server mediante l'eseguibile
*statistiche.sh* il quale effettua il parsing del file di log prodotto dal server.
