## Installation

> La clé privée et le certificat publique du serveur ont été inclu dans le rendu. Cela signifie qu'ils ont été push sur github, et même s'il s'agit d'un repository privé, ce n'est pas une bonne pratique. Je l'ai effectué car je souhaitais que vous n'ayez qu'à lancer une commande pour tester le projet, cependant il est possible de regénérer ces clés en suivant les étapes indiquées ci-dessous.

> Les exécutables pré-compilés de la dernière version ont été également inclu dans le rendu. Cependant, il est possible de les regénérer en suivant les étapes décrites ci-dessous.
 
Installation du package libssl pour le développement :
```shell
sudo apt-get install libssl-dev
```
Création de la clé privée et du certificat pour le serveur :
```shell
openssl req -newkey rsa:2048 -new -nodes -x509 -days 3650 -keyout key.pem -out cert.pem
```
Compilation des exécutables via cmake :
```shell
cd build
cmake ..
make
```
Les exécutables générés par cmake sont ensuite respectivement placés dans les dossiers `server/` et `client/`
Compilation des exécutables via gcc :
```shell
gcc server.c -o server -lcrypto -lssl
gcc client.c -o client -lcrypto -lssl
```

## Usage

Le dépôt github contient tous les fichiers nécessaires pour effectuer des tests. Il est donc possible de tester ce projet simplement en lançant les commandes suivantes :
```shell
./client -p 80 -h 127.0.0.1 ./monitored-folder/
./server -p 80 -l 127.0.0.1
bash generate_file.sh
```
Les fichiers reçus par le serveur peuvent être observés dans le dossier `server/client-files/<client-id>/`, généré automatiquement.
> L'exécutable du serveur doit être placé dans le même dossier que la clé privée et le certificat générés précédemment.

De manière plus générale, les exécutables peuvent être lancés en leur fournissant les arguments suivants :
```shell
./client -p <port> -h <host> <folder1> ...
./server -p <port> -l <host>
```

## Notes

Je vous propose ici une courte explication de ce que j'ai mis en place afin de respecter les consignes.

La plateforme choisie pour développement est linux. Ce projet a été développé sur une machine virtuelle ubuntu 20.04.

La première véritable problématique rencontrée a été l'identification unique des clients. Il existe plusieurs moyens d'identifier une machine de manière unique, comme par exemple l'adresse MAC, la génération d'un UUID stocké sur le client, la lecture des informations matérielles comme les numéros de séries de la carte-mère ou le serial id des disques. Je n'ai pas choisi d'utiliser l'adresse MAC pour plusieurs raisons, comme le fait qu'une machine peut en avoir plusieurs ou qu'il puisse s'agir d'adresses virtuelles réutilisées par d'autres clients (par exemple les adresses MAC docker). J'ai également préféré éviter de créer un fichier côté client pour y stocker son UUID par soucis de discrétion. Enfin, la lecture des informations systèmes nécessitent un accès root dans certains cas, ce qui est la raison pour laquelle je n'ai pas retenu cette solution. Je suis donc finalement parti sur l'usage du fichier /etc/machine-id, contenant un UUID unique généré par linux à l'installation (ou au démarrage s'il n'est pas présent).

La seconde a été la sécurisation de la connexion afin d'assurer la confidentialité des données échangées. Je pensais au départ partir sur un simple chiffrement du fichier à transmettre, mais je me suis rendu compte que j'allais devoir transmettre au serveur plus de données que ce simple fichier. Je me suis donc penché sur l'implémentation d'une connexion SSL/TLS afin de sécuriser tous les échanges effectués. Je n'avais jamais réalisé cette implémentation auparavant en C, mais je l'ai réussi en partie grâce à la documentation officielle d'openssl qui m'a bien aidé.

La troisième problématique rencontrée a été l'intégration d'un système de vérification d'intégrité. Je sais que la pile TCP/IP inclut déjà un système de vérification d'intégrité des paquets basé sur l'algorithme CRC, cependant j'ai pensé que ce ne serait pas suffisant. J'avoue ne pas avoir trop bloqué sur cette problématique : j'ai simplement généré un checksum du fichier avant de l'envoyer, que j'ai ensuite transmis au serveur afin que celui-ci puisse vérifier l'intégrité du fichier tout juste écrit. Si le fichier est corrompu, il est alors renommé, afin qu'il puisse être facilement identifié que le fichier reçu est corrompu.

Enfin, le dernier point bloquant a concerné l'API inotify : en effet, j'utilisais depuis le départ l'événement IN_CREATE afin de détecter la création d'un nouveau fichier au sein des dossiers monitorés. Cependant, au cours de mes tests, j'ai remarqué la présence d'un comportement particulier : les fichiers créés et étant très larges avaient souvent tendance à être corrompu une fois transmis. Cela venait du fait qu'inotify émettait l'événement de création sitôt le fichier créé, et qu'à l'émission de cet événement le contenu du fichier n'était pas encore écrit sur le disque ou ne l'était que partiellement. J'ai donc dans un premier temps ajouté un sleep entre la détection de l'événement et le traitement du fichier, mais je trouvais que cela était une horrible manière de gérer le problème, puisque si un fichier encore plus large était créé, son écriture allait tout simplement prendre plus de temps que le sleep. J'ai donc choisi d'utiliser l'événement IN_CLOSE_WRITE, qui lui est émis une fois qu'un fichier ayant été ouvert en écriture a été fermé. Cela a pour avantage de pouvoir gérer de gros fichiers, l'inconvénient étant que désormais, même les fichiers ayant été modifiés sont transmis au serveur. Je pense que cela pourrait être un axe d'amélioration dans mon programme, mais j'ai manqué de temps pour le traiter.   

Concernant la gestion concurrente des clients côté serveur, j'ai utilisé l'appel système fork() permettant de créer un processus enfant, dans lesquels les traitements de chaque client sont effectués afin de ne pas bloquer les requêtes suivantes.

Je souhaitais également mettre la lumière sur la terminaison des programmes client et serveur : étant donné que cela n'était pas précisé dans les consignes, je suis parti du principe que ceux-ci seraient lancés manuellement. C'est pour cela que pour les arrêter correctement en fermant les sockets et en libérant la mémoire utilisée, j'ai choisi d'intercepter le signal sigint (CTRL+C) afin de procéder à ces actions. Bien entendu, cela peut ne pas correspondre à l'usage voulu dans certains cas, par exemple si le programme est lancé par un autre processus. Il faudrait alors retravailler la manière dont nous souhaitons interrompre notre programme proprement.

Pour terminer, voici les principales librairies sur lesquelles je me suis basé afin de développer ce projet :
- getopt est utilisé côté client et serveur afin de parser les arguments fournis en entrée.
- inotify est utilisé pour monitorer les dossiers côté client.
- openssl est utilisé côté client et serveur, afin de sécuriser la connexion entre les deux parties grâce à TLS.