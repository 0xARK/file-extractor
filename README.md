- Expliquer le choix des librairies
- Expliquer la préférence de ne pas créer de fichier pour être moins intrusif (un fichier aurait pu être créé pour la liste des folders ou pour stocker un id unique à chaque client)
- Expliquer le choix de terminaison du client (pas d'info sur comment le programme devait être lancé/terminé, donc j'ai choisi de faire en sorte qu'il puisse être quitté proprement en libérant la mémoire avant de quitter, même si cela ne peut pas forcément être géré dans tous les cas d'usage, ex si le script est lancé par un autre process ou que le script est lancé dans un SSH)

## Installation

Installation du package libssl pour le développement :
```shell
sudo apt-get install libssl-dev
```
Création de la clé privée et du certificat pour le serveur :
```shell
openssl req -newkey rsa:2048 -new -nodes -x509 -days 3650 -keyout key.pem -out cert.pem
```

# todo
- upgrade socket connection to SSL
- add integrity check in transferred file (maybe with checksum ?)