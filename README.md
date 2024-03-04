## Installation

> La clé privée et le certificat publique du serveur ont été inclu dans le rendu. Cela signifie qu'ils ont été push sur github, et même s'il s'agit d'un repository privé, ce n'est pas une bonne pratique. Je l'ai effectué car je souhaitais que vous n'ayez qu'à lancer une commande pour tester le projet, cependant il est possible de regénérer ces clés en suivant les étapes indiquées ci-dessous.

Installation du package libssl pour le développement :
```shell
sudo apt-get install libssl-dev
```
Création de la clé privée et du certificat pour le serveur :
```shell
openssl req -newkey rsa:2048 -new -nodes -x509 -days 3650 -keyout key.pem -out cert.pem
```

## Notes

Je vous propose ici une courte explication de ce que j'ai mis en place afin de respecter les consignes

La première véritable problématique rencontrée a été l'identification unique des clients. Il existe plusieurs moyens d'identifier une machine de manière unique, comme par exemple l'adresse MAC, la génération d'un UUID stocké sur le client, la lecture des informations matérielles comme les numéros de séries de la carte-mère ou le serial id des disques. Je n'ai pas choisi d'utiliser l'adresse MAC pour plusieurs raisons, comme le fait qu'une machine peut en avoir plusieurs ou qu'il puisse s'agir d'adresses virtuelles réutilisées par d'autres clients (par exemple les adresses MAC docker). J'ai également préféré éviter de créer un fichier côté client pour y stocker son UUID par soucis de discrétion. Enfin, la lecture des informations systèmes nécessitent un accès root dans certains cas, ce qui est la raison pour laquelle je n'ai pas retenu cette solution. Je suis donc finalement parti sur l'usage du fichier /etc/machine-id, contenant un UUID unique généré par linux à l'installation (ou au démarrage s'il n'est pas présent).

La seconde problématique a été la sécurisation de la connexion afin d'assurer la confidentialité des données échangées. Je pensais au départ partir sur un simple chiffrement du fichier à transmettre, mais je me suis rendu compte que j'allais devoir transmettre au serveur plus de données que ce simple fichier. Je me suis donc penché sur l'implémentation d'une connexion SSL/TLS afin de sécuriser tous les échanges effectués. Je n'avais jamais réalisé cette implémentation auparavant en C, mais je l'ai réussi en partie grâce à la documentation officielle d'openssl qui m'a bien aidé.

Enfin, la dernière problématique rencontrée a été l'intégration d'un système de vérification d'intégrité. Je sais que la pile TCP/IP inclu déjà un système de vérification d'intégrité des paquets basé sur l'algorithme CRC, cependant j'ai pensé que ce ne serait pas suffisant. J'avoue ne pas avoir trop bloqué sur cette problématique : j'ai simplement généré un checksum du fichier avant de l'envoyer, que j'ai ensuite transmis au serveur afin que celui-ci puisse vérifier l'intégrité du fichier tout juste écrit.

Concernant la gestion concurrente des clients côté serveur, j'ai simplement utilisé l'appel système fork() permettant de créer un processus enfant, dans lesquels les traitements de chaque client sont effectués afin de ne pas bloquer les requêtes suivantes.

Je souhaitais également mettre la lumière sur la terminaison des programmes client et serveur : étant donné que cela n'était pas précisé dans les consignes, je suis parti du principe que ceux-ci seraient lancés manuellement. C'est pour cela que pour les arrêter correctement en fermant les sockets et en libérant la mémoire utilisée, j'ai choisi d'intercepter le signal sigint (CTRL+C) afin de procéder à ces actions. Bien entendu, cela peut ne pas correspondre à l'usage voulu dans certains cas, par exemple si le programme est lancé par un autre processus. Il faudrait alors retravailler la manière dont nous souhaitons interrompre notre programme proprement.

Pour terminer, voici les principales librairies sur lesquelles je me suis basé afin de développer ce projet :
- getopt est utilisé côté client et serveur afin de parser les arguments fournis en entrée.
- inotify est utilisé pour monitorer les dossiers côté client.
- openssl est utilisé côté client et serveur, afin de sécuriser la connexion entre les deux parties grâce à TLS.