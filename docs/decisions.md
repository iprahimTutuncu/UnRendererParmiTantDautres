La liste des décisions prises durant le projet, avec les raisons qui nous ont poussés à les prendre :

Le but de ce document est de nous aider à documenter la conception du projet et d'expliquer les choix techniques que nous avons faits.

Nos points portent sur l'historique du design et les preuves de conceptions.
Toute décision d'ingénierie doit être ajoutée dans notre rapport final !

Nous voulons des tableaux et des critères pour mieux expliquer les choix techniques que nous avons faits.

## Éléments ou pistes de décision qui ont été pris avant la première revue de conception (avant le 2 juin)

- Nous allions utiliser Vulkan en brut, puis nous sommes passés à SDL3 avec Vulkan.
- Utilisation de CMake pour la compilation du projet.
- Compilation multiplateforme pour Windows et Linux.
- Nous sommes tous partis chacun de notre côté pour travailler sur le projet.
- Nous avions un moteur 3D avec une architecture initiale.
- Au début, nous avions plusieurs stratégies de simulation. Nous avons décidé de prendre la plus difficile. Nous nous rendons compte que c'est plus difficile que prévu. Peut-on revenir à MLS, MPM, etc. ?

- Des tests ont été effectués avec le type de renderer que nous voulons utiliser. Certains tests ont été faits avec un DeferredRenderer pour voir si cela suffit dans notre cas.
- DeferredRendering : pourquoi ne pas avoir simplement pris un shader ? Pourquoi l'un versus l'autre ? (à expliquer avec un tableau et des critères)

- On peut faire un diagramme de séquence entre ImGui et SDL pour savoir quand et quoi s'appelle.

## Éléments ou pistes de décision qui ont été pris après la première revue de conception (après le 2 juin)

- Nous avons fusionné les projets en un seul projet.
- Nous avons une nouvelle architecture.
- Nous avons/allons définir une liste de restrictions et de code style pour le projet.
