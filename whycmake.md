### Pourquoi avoir choisi CMake ?

Notre projet doit fonctionner sur plusieurs systèmes d'exploitation, ce qui nécessite une approche cross-platform. Nous voulions également un système de build unifié pour tous les membres de l'équipe, quel que soit leur environnement de développement.

Au début, le projet contenait à la fois un fichier CMakeLists.txt et un premake.lua qui construisait un workspace Visual Studio. Maintenir les deux à jour en parallèle aurait demandé des efforts constants pour s'assurer de leur conhérence l'un avec l'autre. La gestion des bibliothèques externes posait aussi problème. Le projet embarquait des biliothèques au format .lib pour ceux qui utilisait windows et findpackage pour ceux qui utilisait linux. Cette approche fait que nous n'étions pas certains de tous utiliser la même version des librairies. On avait même plusieurs librairies non utilisé dans le code.

CMake répond à ces problèms en unifiant les différences entres les platformes à travers un format unique, simple et extensible.

Ses avantages:
- Détection automatique des programmes, bibliothèques et fichiers d'en-tête nécéssaires au build, en tenant compte des variables d'environnement et du registre Windows.
- Support du build out-of-source, permettant de séparer les fichiers générés des sources. Cela facilite le nettoyage et évite tout suppression accidentelle de code source.
- Facilité à créer des étapes de build complexes, comme la compilation de shaders.
- Gestion des composants optionnels dès la phase de configuration.
- Compilation possible de plusieurs executables ou cas de test au sein d'un même projet.
- Passage simple entre build statique et dynamique, avec prise en charge transparente des drapeaux spécifiques aux plateformes.
- Support natif du build parallèle.

Tout ça nous garanti que le projet se compile de manière identique sur toutes les machine. Ça va aussi facilité la mise en place d'un pipeline CI fiable pour valider automatiquement la compilation et les convention de code.


---


// TODO: comparer avec le relatif à ce qu'on avait au début
// TODO: corriger les fautes ;-;
