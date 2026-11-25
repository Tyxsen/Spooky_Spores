# Spec — Despawn des drops au sol
Item TODO : 6 (partie 6b) | Branche : feature/ui-player-controller | Statut : brouillon

## Objectif
Timéo : « tout type de drop, doit disparaitre au bout d'un certain moment, (disons 5 min). » Évite l'accumulation d'acteurs physiques au sol. Indépendant de `ui-player-controller.md` (6a), commit séparé.

## Comportement
- `AResourceDrop::DropLifetime` (float, secondes, `EditAnywhere`, défaut `300.0f`). À la fin de `BeginPlay`, `SetLifeSpan(DropLifetime)` si `DropLifetime > 0`. `<= 0` = pas de despawn (échappatoire de réglage, pas une règle de jeu).
- S'applique à **tout** drop (tous les `EResourceType`, tous les chemins : nœud récolté, champ récolté par le joueur, butin ennemi), puisque la durée est posée par le drop lui-même. Drops du joueur conservés : `SpawnDrops` n'est pas modifié (Timéo : « Oui quand c'est le joueur je veux des drops quand même. »). La récolte du serviteur ne crée toujours aucun drop.
- Spawn différé respecté : `BeginPlay` s'exécute au `FinishSpawning`, après `InitializeDrop` ; le décompte part de ce moment (pas du jour du `SpawnActorDeferred`).
- Expiration : `Destroy()` silencieux, sans crédit ni remboursement, sans évènement, sans effet visuel (« Plus tard » dans le TODO). Le drop disparaît même s'il n'a pas atterri ou si `PickupDelay` n'est pas écoulé.
- Ramassage avant expiration : `TryCollect` crédite puis `Destroy()` ; la destruction annule le minuteur de durée de vie, aucune seconde destruction ni second crédit. Mécanisme : `SetLifeSpan` du moteur, aucun minuteur maison (à vérifier par l'implémenteur : `AActor::EndPlay` / `Destroy` nettoie le timer de vie).
- Pause du jeu (menu des serviteurs) : le monde ne tourne pas, la durée de vie ne s'écoule pas pendant la pause (comportement attendu du timer moteur, à confirmer en PIE).
- **Pas saisissable** (Timéo : « Un drop ne peut pas être tenu en télékinésie, un drop est un objet au sol qui peut seulement être ramassable par le joueur. »). Vérifié : `AResourceDrop` dérive de `AActor` seul, n'implémente pas `IGrabbable` (seul `APhysicsGrabbable` le fait) et son preset est `ResourceDrop`, pas `Interactable` : aucun code à ajouter, aucune contradiction côté C++. Non vérifiable ici : un BP enfant de drop pourrait avoir ajouté l'interface ou changé le preset (à contrôler par `bp-integration-planner`).

## Critères d'acceptation
- [ ] Un drop dont `DropLifetime` vaut `D` est invalide (détruit) après que le monde a avancé de plus de `D` secondes, et encore valide un peu avant `D`.
- [ ] Le défaut de la classe vaut `300.0f` (lu sur le CDO).
- [ ] `DropLifetime <= 0` : `GetLifeSpan()` vaut 0 (infini) et le drop survit à un long avancement du monde.
- [ ] Le décompte part de `FinishSpawning` : un drop créé par `SpawnActorDeferred` puis finalisé après un délai de monde a bien sa pleine durée restante au `FinishSpawning` (`GetLifeSpan() == DropLifetime`).
- [ ] Un drop expiré ne crédite aucun compteur, même avec un pawn équipé d'un `UResourceCounterComponent` hors de portée de ramassage ; le compteur reste inchangé.
- [ ] Un drop ramassé avant expiration crédite exactement une fois (`Type`, `Amount`), puis l'avancement du monde au-delà de `D` ne change plus le compteur.
- [ ] `DropLifetime` plus court que `PickupDelay` : le drop est détruit à l'expiration sans erreur ni ramassage tardif.
- [ ] `AResourceDrop` n'implémente pas `UGrabbable` (`GetDefaultObject()->Implements<UGrabbable>()` faux) et son preset de collision n'est pas `PhysicsInteractable` / type d'objet `Interactable`.
- [ ] `AResourceDrop::SpawnDrops` produit toujours un drop par entrée valide (`SpawnCount` respecté) : le comportement existant est inchangé.

## API exposée
Aucune fonction nouvelle. `UPROPERTY(EditAnywhere, Category = "Resources|Lifetime", meta = (ClampMin = "0.0", Units = "s")) float DropLifetime = 300.0f;` (protégée, groupe PROPERTIES). Les tests lisent l'état par `GetLifeSpan()` et `IsValid`, et fixent la durée via une sous-classe de test (membre protégé).

## Valeurs réglables
`DropLifetime` : 300 s.

## Cas limites
- Drop tombé hors du monde ou bloqué en l'air (non atterri) : expire quand même, ce qui sert de filet.
- Drop recyclé par un BP qui appelle `Destroy` lui-même avant l'expiration : aucun effet de bord (timer annulé).

## Périmètre de fichiers
- **Modifier** : `Resources/ResourceDrop.h`, `Resources/ResourceDrop.cpp` (ajout de `SetLifeSpan` en fin de `BeginPlay` ; inclusions explicites si besoin).
- **Créer** : `Tests/ResourceDropLifetimeSpec.cpp` (sous-classe de test en tête de fichier).
- **Points chauds** : aucun (`ResourceTypes.h` non modifié). Éditeur : aucun changement requis ; contrôler les BP enfants de drop (pas d'`IGrabbable`, pas de preset `PhysicsInteractable`, pas de surcharge de `DropLifetime`).

## Hors périmètre
- Effet visuel ou sonore de disparition (fondu, particules) : « Plus tard » du TODO.
- Durée différente par type de drop (une seule durée pour tous, comme décidé).
- Attraction ou fusion des drops (« Idées à rediscuter »).
- Persistance des drops dans la sauvegarde (5b).

## Points d'incertitude
- Le nettoyage du timer de vie à `Destroy` et son arrêt pendant la pause sont des comportements du moteur que je n'ai pas relus dans le code UE 5.8.3 : l'implémenteur doit les vérifier avant de s'y fier.
- Les BP enfants de drop n'ont pas été inspectés.
