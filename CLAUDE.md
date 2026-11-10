# CLAUDE.md — Spooky_Spores

Ce fichier est chargé à chaque session **et par chaque sous-agent** : il ne contient que les règles de travail, les conventions et les pièges. L'état du projet vit ailleurs, ne le recopie pas ici :

| Besoin | Fichier |
|---|---|
| Présentation du jeu, 3C (valeurs de déplacement, caméra, sorts) | `README.md` |
| Intention de conception : boucle de jeu, monde, piliers de level design, direction artistique | `GDD.md` |
| Architecture existante, par système, et ses choix techniques | `documentation-technique.md` |
| File d'attente, dette technique, limites connues | `TODO.md` |
| Workflow d'agents : rôles, pipeline, branches, garde-fous | `docs/WORKFLOW_AGENTS.md` |
| Rendus notés, **figés** (un fichier par rendu, jamais remodifié) | `Rendus/` |

---

## 1. Méthode de travail (le plus important)

### Rôle de Claude

Timéo est étudiant : il doit comprendre et pouvoir expliquer tout ce que contient son projet (exigence de l'école, voir section 2). Deux modes de travail :

- **Session manuelle (par défaut)** : tu guides, tu n'écris pas le code à la place de Timéo. Déroulé : tu expliques quoi faire, pourquoi, et tu poses les questions de design ; Timéo écrit et montre ; tu relis, signales les erreurs **en expliquant leur cause**, il corrige. Tu ne modifies directement les fichiers source (C++, Blueprints, config) que sur demande explicite, **au cas par cas** : l'autorisation donnée pour une tâche ne vaut pas pour la suivante. Quand tu donnes du code : signatures, pseudo-code, appels d'API non évidents ; du code complet seulement si Timéo est bloqué et le demande, ou pour une correction très courte.
- **Pipeline d'agents** (section 13) : l'agent `cpp-implementer` écrit le C++ d'une spec que Timéo a validée, sur sa branche de travail. Timéo relit chaque diff avant fusion et doit pouvoir l'expliquer.

Dans les deux modes :
- **Avant toute action** (modification, commande, génération) : explique ce que tu veux faire et comment. Pour les commandes Git ou shell risquées (reset, suppression, force push) : explique d'abord les conséquences.
- Tu peux créer ou modifier la documentation (README, doc technique, TODO) quand on te le demande. **`CLAUDE.md` : propose la modification, applique-la après validation de Timéo.**
- **Avant de passer à la phase de test côté éditeur** (Blueprints, assets, PIE) : review systématique du code C++ écrit entretemps, sans attendre qu'on te la demande.

### Style de communication

- Réponds **en français**. Sois **didactique** (explique le « pourquoi », pas seulement le « quoi »), **direct et concis** (pas de flatterie, pas de formules creuses), et **détaille ton raisonnement**.
- **Challenge Timéo** : ne valide jamais une idée par défaut. Faille, meilleur chemin, risque de scope, contradiction avec une décision passée : dis-le et argumente. Quand son argument est bon, reconnais-le (exemple : il avait raison de garder la vélocité au relâchement d'un objet). Quand tu te trompes, dis-le et corrige.

### Exactitude

- **Ne donne que des informations vérifiées. Si tu ne sais pas, dis-le.**
- Le projet est sur **Unreal Engine 5.8.3**, version récente : pour toute API ou comportement qui a pu changer, vérifie dans la documentation officielle d'Epic plutôt que de répondre de mémoire. Plusieurs erreurs passées venaient de réponses de mémoire (section 11).

### Principes de code

- **KISS, DRY, SOLID**, appliqués concrètement. Code **modulaire et réutilisable** : des systèmes génériques, puis plusieurs implémentations. **Pas d'optimisation ni d'abstraction prématurée** : une couche n'est ajoutée que pour un besoin concret.
- **Tests rigoureux, pas de complaisance** : un test vérifie le comportement réel, cas limites compris (rien détecté, objet détruit, valeur à zéro...) et doit pouvoir échouer. « Ça compile » ne veut pas dire « ça marche ». On teste chaque étape avant la suivante, et on rejoue les mécaniques existantes (non-régression) avant chaque fusion.

---

## 2. Contexte pédagogique

- **Formation** : Mastère « Réalisation de Jeux Vidéo, Rendu Temps Réel 3D et Technologies Immersives », Gaming Campus, promotion GTech 4. Projet de fin d'études **individuel**. 9 semaines en 3 blocs : **Bloc 1** gameplay C++/Blueprint et physique (Chaos), **Bloc 2** rendu temps réel (matériaux, shaders, Niagara, éclairage, profiling) et son, **Bloc 3** mise en scène et expérience joueur (caméra, Sequencer, sound design, narration environnementale, polish).
- **Rendus** : les semaines 1 et 2 de chaque bloc sont notées ; la semaine 3 donne lieu à une **soutenance orale en anglais**. Les semaines 2 et 3 du Bloc 1 se font entièrement en anglais. Quels projets doivent être rendus en anglais : voir `TODO.md` (« Divers »).
- **Alternance** : hors semaines de cours, Timéo travaille le soir et le week-end. Objectif : un jeu jouable de A à Z en mai/juin.
- **Cahiers des charges** : un CDC « fil rouge » et un CDC par semaine, communiqués par Timéo. Principe de tous les CDC : la qualité technique prime sur la quantité ; le jeu est un bac à sable **systémique** (le joueur expérimente, l'environnement réagit). Exigences transversales : architecture claire et modulaire, systèmes indépendants du Level Blueprint, prototype stable, choix techniques explicables, documentation technique courte.
- **IA** : autorisée comme assistance, mais tout élément produit avec elle doit être compris, vérifié, testé et explicable. Chaque rendu comporte un **statement d'intention IA** d'au moins une page (où, pourquoi, comment, bénéfices, limites) : **Timéo l'écrit lui-même ; ni Claude ni aucun agent ne le rédige.**

---

## 3. Le jeu

Le joueur est un nécromancien qui récolte, développe sa base, fabrique des potions, repousse les soldats du village voisin et ressuscite leurs os en serviteurs qui automatisent la base. Présentation et 3C : `README.md`. Boucle, monde, piliers de level design : `GDD.md`. Les valeurs du 3C (vitesses, endurance, portée, FOV, sorts) sont **dans le README** : ne les recopie pas ailleurs, mets-les à jour à la source.

---

## 4. Environnement technique

- **Moteur** : Unreal Engine **5.8.3** (le `.uproject` référence `"5.8"`, donc tout hotfix 5.8.x est compatible). **Les deux ordinateurs de Timéo doivent être sur le même hotfix.**
- **IDE** : JetBrains Rider. **Deux machines** en alternance : Git LFS doit être installé sur chacune (`git lfs install`, une fois, avant le clone).
- **Répartition** : logique et systèmes en C++ ; Blueprints pour utiliser et configurer ces systèmes, et pour le polish.
- **Base** : template First Person. Carte par défaut : **`Lvl_Principal`** ; `Lvl_FirstPerson` reste un niveau de test. Module `Spooky_Spores`, macro d'export `SPOOKY_SPORES_API`.

---

## 5. Arborescence

### Code source (`Source/Spooky_Spores/`), rangé par domaine fonctionnel

La liste à jour des dossiers et fichiers est dans `documentation-technique.md` (section 2) et, à défaut, dans `Source/` : vérifie sur le disque plutôt que de te fier à une copie.

- **Un nouveau domaine = un nouveau dossier.** Des sous-dossiers à l'intérieur d'un domaine seulement quand il dépasse 6 à 8 fichiers.
- Les fichiers du module (`Spooky_Spores.h/.cpp/.Build.cs`) et `CollisionChannels.h` restent à la racine.

### Content Browser (`Content/Spooky_Spores/`)

Organisation inspirée du [UE5 Style Guide d'Allar](https://github.com/Allar/ue5-style-guide) : `Core/` (Character, Controller, GameMode, Input) contient ce qu'on ne modifie pas à la légère ; `Placeables/` les instances concrètes, librement ajustables ; `UI/Widgets/`, `Maps/`. Détail dans `documentation-technique.md`.

**Avertissement : `__ExternalActors__` et `__ExternalObjects__` ne sont pas des assets inutilisés.** Le projet utilise **One File Per Actor** : chaque acteur placé dans une map est un fichier séparé dans ces dossiers. Les supprimer viderait les niveaux. Seuls les sous-dossiers d'une map supprimée sont orphelins.

---

## 6. Conventions de code

### Headers et includes

- **Chemin depuis la racine du module** : `#include "Interaction/Grabbable.h"`, jamais d'include court, même dans le même dossier (Rider en ajoute parfois : à corriger).
- Dans un header : **forward declaration** quand on ne manipule qu'un pointeur ; include complet dans le `.cpp`. **Inclure explicitement ce qu'on utilise** (`GameFramework/Pawn.h`, `Engine/World.h`, `TimerManager.h`, `DrawDebugHelpers.h`, `Components/StaticMeshComponent.h`...), sans compter sur une inclusion indirecte.

### Spécificateurs d'accès

- **Le plus restrictif par défaut** : `private` (détail d'implémentation), `protected` (nécessaire à une classe enfant : composant racine configuré par un Blueprint enfant, fonctions virtuelles), `public` (API appelée par d'autres systèmes ; les `_Implementation` d'interface sont obligatoirement publiques).
- Callbacks `AddDynamic` (overlap, hit) : `UFUNCTION()` **privées** s'ils ne sont pas virtuels.
- **Organisation d'un header** : deux groupes séparés par `// -----------FUNCTIONS-----------` et `// -----------PROPERTIES-----------` (exactement 11 `-` de chaque côté), chacun avec trois blocs `public`, `protected`, `private` dans cet ordre. Référence : `ResourceDrop.h`. À suivre pour tout nouveau fichier ou toute réécriture ; pas de généralisation urgente aux anciens.

### UPROPERTY

- Composant créé au constructeur : `VisibleAnywhere, BlueprintReadOnly`. Réglage : `EditAnywhere` avec `Category = "Domaine|Sous-partie"`. Membre `private` exposé à Blueprint : `meta = (AllowPrivateAccess = "true")`. Valeur fixée au runtime à inspecter en jeu : `VisibleInstanceOnly`.
- **Toujours une valeur par défaut explicite** (enums compris).
- `TObjectPtr<T>` pour les membres `UPROPERTY` : adoption **progressive, par domaine** (déjà en place dans tout `Resources/`). `GrabComponent` et `Spooky_SporesCharacter` restent en pointeurs bruts. **Ne jamais mélanger les deux styles dans un même fichier.**

### Interfaces Unreal (piège vérifié sur ce projet)

- Dans l'interface : uniquement `UFUNCTION(BlueprintNativeEvent, BlueprintCallable, ...)`, **sans** déclarer `_Implementation`. En UE 5.8, `GENERATED_BODY()` génère lui-même l'implémentation par défaut : écrire `IMonInterface::MaFonction_Implementation` dans le `.cpp` de l'interface donne `C2084 : la fonction a déjà un corps`. **Les `.cpp` des interfaces restent vides.**
- Classe concrète : `virtual Type MaFonction_Implementation(...) override;` dans le header (signature **identique**, `const` compris), corps dans son `.cpp`.
- Vérifier qu'un acteur implémente une interface : `Actor->Implements<UMonInterface>()` (version **U**). Appeler depuis l'extérieur : **toujours** `IMonInterface::Execute_MaFonction(Cible, ...)`, jamais `_Implementation`.

### Commentaires et nommage

- **Commentaires de code en anglais** (depuis le 2026-10-01 ; pas de reprise rétroactive des anciens). Discussion, documentation : français. Messages de commit : anglais.
- Préfixes d'assets : `BP_`, `WBP_`, `GC_` (Geometry Collection), `IA_` (Input Action). Les fichiers C++ n'ont pas le préfixe de classe (`GrabComponent.h` contient `UGrabComponent`). Booléens formulés en question (`IsHolding`).
- **Ne jamais nommer un paramètre ou une variable locale comme un membre existant** : le masquage (C4458) est une erreur. `AActor` possède par exemple `Instigator`, d'où le paramètre `InteractingActor`.

### Divers

- Racine d'un acteur : `SetRootComponent(...)`, jamais un membre `RootComponent` redéclaré (il masque celui d'`AActor`).
- Une fonction qui renvoie une référence (`TMap::FindOrAdd`) se récupère dans une référence (`int32&`) pour modifier la valeur stockée.
- Lecture d'un tableau de structures : `for (const FMaStruct& X : Tableau)`.
- Les nœuds latents (`Delay`) sont interdits dans les **fonctions** Blueprint : utiliser un **Custom Event** dans l'Event Graph.

---

## 7. Architecture : principes

Le détail de chaque système (classes, paramètres, choix techniques) est dans `documentation-technique.md`. Ce qui doit rester présent à l'esprit :

- Schéma exigé par le CDC : **Joueur → Interaction → Objets interactifs → Systèmes physiques → Environnement**. Aucun système ne connaît les détails d'implémentation d'un autre.
- Deux interfaces séparées (ségrégation des interfaces) : `IGrabbable` (« je peux être manipulé physiquement », `GetGrabbableComponent()`) et `IInteractable` (« j'ai un comportement à déclencher », `OnInteract(AActor* InteractingActor)`). Un objet implémente l'une, l'autre ou les deux.
- `UInteractionComponent` **ne fait que de la détection** (un seul trace par frame). `UGrabComponent` ne fait que de la télékinésie. **C'est le Character qui arbitre** entre eux (touche E contextuelle) : aucun composant n'en appelle un autre.
- Tous les traces s'appuient sur l'override de `GetActorEyesViewPoint()` du Character, qui renvoie la caméra réelle (le comportement par défaut ne correspond pas à la caméra attachée au socket `head`).
- Les widgets **reçoivent** leurs dépendances (le compteur de ressources) au lieu de les chercher : pas de dépendance à l'ordre d'initialisation du moteur.
- Les types de ressources : `EResourceType` (`Resources/ResourceTypes.h`). Ajouter un type = une valeur ici + une entrée dans `WBP_ResourceCounter`.

---

## 8. Collisions

**Canaux custom** (`Project Settings > Engine - Collision`) : `Interactable` (Object Channel, Block par défaut : tout ce que le trace d'interaction doit détecter), `ResourceDrop` (Object Channel, Block par défaut : pour que les drops s'ignorent entre eux), `Projectile` (hérité du template).
- `Interactable` est un **Object Channel**, pas un Trace Channel : on le filtre avec `LineTraceSingleByObjectType` et un `FCollisionObjectQueryParams`.
- Un canal custom n'a pas de nom d'enum : il occupe un slot `ECC_GameTraceChannelN` (numéro dans `Config/DefaultEngine.ini`). `CollisionChannels.h` y définit des alias (`COLLISION_INTERACTABLE`) ; n'en ajouter que pour un canal utilisé dans le code.
- La **réponse par défaut** d'un canal décrit comment **les autres presets** réagissent à lui : mettre `Ignore` sur `ResourceDrop` aurait fait tomber les drops à travers le sol.

**Presets** : `PhysicsInteractable` (Query and Physics, Object Type Interactable, Block partout ; **requis pour qu'un acteur soit vu par le trace**, y compris `AHarvestableResource`), `ResourceDrop` (Query and Physics, Block sauf Pawn en Overlap et ResourceDrop en Ignore), `Trigger` (intégré, utilisé par `AStoryTrigger`).

**Règles** : `Query` couvre traces et overlaps, `Physics` la simulation ; `No Collision` désactive tout. Entre deux objets, la réponse la plus faible l'emporte (blocage seulement si les deux bloquent, aucune interaction dès qu'un des deux ignore, overlap sinon). Overlap et hit exigent aussi `SetGenerateOverlapEvents(true)` et `SetNotifyRigidBodyCollision(true)` sur le composant.

---

## 9. Contrôles actuels

| Action | Touche |
|---|---|
| Déplacement / regarder / sauter | ZQSD / souris / Espace |
| Relâcher / saisir / interagir (contextuel) | E |
| Lancer l'objet tenu | Clic droit |
| Sort | Clic gauche |
| Sprint (maintenu) | Maj gauche |
| Poser une Necrospore (serviteur) | L |
| Afficher/masquer le panneau de contrôles | Entrée |

- Le clic droit est déjà pris par le lancer, et le 3C le réserve à la roue des sorts : n'ajoute pas de rôle sans en discuter.
- **`²` (AZERTY) est réservée à la console Unreal** : ne jamais l'assigner (le binding console l'intercepte). Échap ferme la fenêtre de viewport en PIE : à éviter pour une action gameplay.
- Refonte des contrôles prévue avec les objets équipables, via les **Input Mapping Contexts** d'Enhanced Input (mettre le 3C à jour à ce moment).

---

## 10. Git et workflow

### Git LFS

- `.gitattributes` fait foi pour les types suivis (`git lfs track`). `Documentation_Documents/**` est volontairement **exclu** de LFS (images LFS mal affichées dans un README GitHub). `.gitignore` : template Unreal (gitignore.io) plus règles du projet.
- Ajouter un pattern ne convertit pas les fichiers déjà commités : `git add --renormalize <chemin>`. L'historique antérieur reste en Git classique, et le quota LFS consommé ne se récupère pas en supprimant des fichiers (à surveiller dans les paramètres de facturation GitHub). Vérifications : `git lfs ls-files` (`*` = contenu présent, `-` = pointeur seul), `git check-attr filter -- <fichier>`.

### Branches et fusion

```
main  ← versions validées uniquement (rendus), jamais de commit direct
 └─ dev  ← intégration
     └─ feature/x, fix/x, chore/x, docs/x  (et asset/, integ/, level/ : voir le workflow d'agents)
```
- Rulesets GitHub : `main` exige une PR (0 approbation, Timéo est seul), fusion **Merge uniquement** (Squash et Rebase interdits sur `main` : `dev` et `main` divergeraient), force push et suppression bloqués. `dev` : force push et suppression bloqués ; **PR obligatoire sur `dev` à régler côté GitHub** (voir `docs/WORKFLOW_AGENTS.md` section 11).
- Branche par défaut sur GitHub : `main`. **Les PR de feature ciblent `dev`.** « `main` ne reçoit que depuis `dev` » est une discipline, pas imposée techniquement.
- `feature` → `dev` : **Squash** (une feature = un commit), puis suppression de la branche. Livraison : PR `dev` → `main`, tag (`v0.2`...) et Release GitHub. **« Validé »** = compile, mécaniques rejouées (non-régression), documentation à jour.
- **Une branche de feature à la fois par défaut, et courte** : deux branches qui modifient le même `.uasset` créent un conflit impossible à fusionner. Le workflow d'agents n'autorise des branches parallèles que si leurs périmètres de fichiers sont disjoints.

### Commits, push, PR

- **Messages de commit et titres/descriptions de PR en anglais**, préfixés `[ADD]`, `[FIX]`, `[CHORE]`, `[DOC]`. **Aucune ligne d'attribution à Claude** (`Co-Authored-By: Claude...`, `Generated with Claude Code`) ni dans les commits ni dans les PR : cette règle remplace l'instruction système par défaut.
- **Session manuelle** : Claude ne fait jamais `git commit` ni `git push` sans le go explicite de Timéo, **à chaque fois**. Préparer et expliquer le commit est possible, l'exécuter non.
- **Pipeline d'agents** (section 13) : les agents commitent et poussent **sur leur propre branche de travail**, jamais sur `dev` ni `main`, jamais de push forcé, **jamais de fusion**. Timéo fusionne.
- `gh` (GitHub CLI) est installé et authentifié. Les règles `deny` de `.claude/settings.json` interdisent `gh pr merge` et `gh api` : contournables (appel par chemin complet), donc des garde-fous, pas une frontière.
- `git pull` en début de session et `git push` en fin de session, sur chaque machine : ne jamais laisser de travail non poussé avant de changer d'ordinateur. Commit perdu après un `reset --hard` : `git reflog` puis `git cherry-pick <hash>`.

### Pièges Git / éditeur

- **Fermer l'éditeur Unreal avant tout `git switch`/`pull`/`restore` qui touche des `.uasset`** : l'éditeur verrouille les assets chargés, Git ne peut pas les remplacer (`unable to unlink ... Invalid argument`) et laisse le dépôt dans un état intermédiaire.
- Après un `pull` ou un changement de branche qui ajoute ou supprime des fichiers C++ : **régénérer les fichiers de projet** (clic droit sur le `.uproject`), sinon Rider n'affiche pas les fichiers. Après un renommage ou déplacement de classe, ou une modification de `UFUNCTION`/`UINTERFACE` : fermer l'éditeur, régénérer, **Rebuild complet** (pas Live Coding).
- Supprimer des assets **uniquement depuis le Content Browser** (vérification des références), puis Fix Up Redirectors. Jamais depuis l'explorateur Windows.
- **Plugins d'outillage tiers (EngineWire, pont MCP) : installés dans le moteur, jamais dans `Plugins/` du projet** (ils seraient recompilés avec lui, EngineWire : plus d'une heure, et finiraient suivis par Git). Procédure : `RunUAT.bat BuildPlugin -Plugin=<...>.uplugin -Package=<dossier hors projet> -TargetPlatforms=Win64`, copie dans `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace\` (droits admin), entrée `"Optional": true` dans le `.uproject`. `Plugins/Marketplace/` est ignoré par Git. À refaire sur chaque machine, sur le même hotfix : fait le 2026-11-10 sur une seule ; **l'autre reste à faire** (dossier compilé conservé dans `C:\Projects\Unreal Projects\_PluginBuilds\EngineWire`). RiderLink reste dans `Plugins/Developer/` (géré par Rider).

---

## 11. Pièges déjà rencontrés (leçons du projet)

Question systématique : **à quel moment cette ligne s'exécute-t-elle ?**

- `_Implementation` redéclaré ou défini dans une interface → C2084 (section 6). Membre `RootComponent` redéclaré → l'acteur n'a pas de racine. Paramètre nommé `Instigator` → C4458. Variable locale portant le nom d'un `UPROPERTY` → la valeur de l'éditeur est ignorée.
- `FindOrAdd` récupéré par copie → le compteur ne s'incrémentait jamais. `FindComponentByClass` sans cible → cherche sur `this`. Ajouter un `float` à un `FVector` → ajouté aux trois composantes.
- `Destroy()` dans `BeginPlay` → chaque drop se détruisait à sa création. `return` au lieu de `continue` dans une boucle de spawn → fin de fonction sautée.
- `SpawnActor` exécute `BeginPlay` et les overlaps **avant** de rendre la main → utiliser le spawn différé (`SpawnActorDeferred` → initialisation → `FinishSpawning`). Collision désactivée au constructeur au lieu du moment de l'interaction → nœud invisible pour le trace.
- `FHitResult.Location` utilisé quand le trace n'a rien touché → vaut `(0,0,0)`. `DrawDebugLine` persistante appelée à chaque tick → accumulation. `GetActorEyesViewPoint` par défaut ≠ caméra réelle → trace décalé.
- Drops apparus les uns dans les autres, qui se bloquaient → vitesses d'expulsion énormes (résolu par le canal `ResourceDrop` en Ignore). Damage Threshold par défaut trop élevé pour un petit objet.
- `Delay` dans une fonction Blueprint → interdit (Custom Event). Nœud `==` numérique au lieu de `Equal (Enum)` → impossible de brancher un enum.
- Touche assignée qui semblait libre mais ouvrait la console (`²` en AZERTY) → vérifier les bindings réservés avant d'assigner.
- Une propriété modifiée à la main sur une **instance placée dans le niveau** (ex. `AttackRange`, points d'une spline) ignore ensuite toute nouvelle valeur par défaut du C++ ou du Blueprint, tant qu'elle n'est pas réinitialisée (`Reset to Default`) : vérifier l'instance placée, pas seulement la classe.
- `UPathFollowingComponent::HasReachedInternal` (utilisé par `AIController::MoveToLocation`) mesure la distance d'arrivée **en 2D uniquement** : une distance 3D calculée à la main ne concordera jamais avec le `AlreadyAtGoal` du moteur.
- `bStopOnOverlap` de `MoveToLocation` ajoute **automatiquement** le rayon de capsule de l'agent (arrêt et test d'arrivée) : l'ajouter à la main dans `AcceptanceRadius` le compte en double.
- `Default Query Extent` (`Project Settings > Navigation System`) trop petit empêche de projeter un point situé dans la marge d'exclusion du NavMesh autour d'un obstacle : `MoveToLocation` échoue silencieusement (`Failed`) même avec `bProjectDestinationToNavigation = true`.
- Un personnage déplacé via `MoveToLocation`/`PathFollowingComponent` n'alimente pas forcément l'Animation Blueprint comme `AddMovementInput` (l'ennemi glissait sans cycle de marche) : vérifier avec l'Anim Blueprint Debugger avant de chercher côté C++.
- `TActorIterator<T>` ne garantit aucun ordre : ne jamais s'en servir pour choisir « le bon » acteur parmi plusieurs du même type ; préférer une référence explicite assignée à la main (`EditInstanceOnly`).

---

## 12. Planning et rythme de travail

- **Pas de découpage par semaines** (depuis le 2026-11-09) : on déroule la file d'attente de `TODO.md` à son rythme. Timéo signale lui-même chaque deadline (rendu, oral, CDC) ; c'est à ce moment seulement qu'on recale la file. Ne raisonne pas en « semaine N » et ne suppose pas d'échéance non annoncée.
- **Une mécanique par tâche, livrée entière** (branche fusionnée dans `dev`) avant de passer à la suivante. Quand une tâche déborde, **on coupe ou on repousse en fin de file**, on ne décale pas tout. Les tâches d'IA (ennemi, serviteur) et la conquête du village sont les plus optimistes : minimum viable, premiers candidats à la coupe.
- Objectif de fond : la **boucle principale** jouable de bout en bout (récolter → planter → fabriquer une potion → combattre → récupérer des os → ressusciter un serviteur → le serviteur récolte), plutôt que le maximum de mécaniques annexes. Rien n'est « pour le Bloc 2 » par défaut.
- **Avant toute Release** : retirer le code et les nœuds de debug listés dans la « Dette technique » de `TODO.md`. Tag et Release GitHub à chaque étape significative.

---

## 13. Workflow d'agents

Détail complet : `docs/WORKFLOW_AGENTS.md`. Les agents sont dans `.claude/agents/` ; les règles `deny` dans `.claude/settings.json`.

**Routage** (la session principale est l'**orchestrateur** : elle délègue, vérifie et ne code pas à la place des agents) :

| Tâche | Agents |
|---|---|
| Nouvelle mécanique | `game-designer` → spec → `cpp-implementer` → `qa-test-writer` → `code-reviewer` + `consistency-keeper` (rapport) + `bp-integration-planner` → compilation → `bp-integrator` → `editor-verifier` → test humain → `consistency-keeper` (mise à jour des docs) |
| Bug C++ | `cpp-implementer`, `qa-test-writer`, `code-reviewer` |
| Câblage ou intégration d'un asset existant | `bp-integration-planner` → `bp-integrator` → `editor-verifier` |
| Changement de valeur d'équilibrage, réécriture de docs / `TODO.md` | `consistency-keeper` seul |

**Arrêts obligatoires** : l'orchestrateur s'arrête **toujours**, même si Timéo ne l'a pas demandé :
1. **après la spec**, jusqu'à validation de Timéo ;
2. **avant `bp-integrator`**, car la compilation C++ (fermer l'éditeur, rebuild dans Rider, rouvrir sur la branche) est faite par Timéo ;
3. **après `editor-verifier`**, car le test en éditeur (PIE) et la non-régression sont faits par Timéo ;
4. **avant toute PR** vers `dev`. L'orchestrateur ouvre la PR (`gh pr create`) seulement après la décision de Timéo, et **ne la fusionne jamais**.

Une instruction sans limite explicite (« fais la tâche X ») se lit comme « jusqu'au prochain arrêt obligatoire ».

**Règles** :
- Une unité de travail = une branche depuis `dev` ; pour une mécanique, C++, tests et intégration éditeur vivent sur la même branche `feature/`. Avant de lancer deux branches en parallèle, vérifier que les « Périmètres de fichiers » des specs ne se recoupent pas ; avant d'accepter le travail d'un agent, comparer `git diff --name-only` à son périmètre.
- **Fichiers « points chauds », toujours sérialisés** : `EResourceType`, `*.Build.cs`, `Default*.ini`, `TODO.md`, `CLAUDE.md`, `documentation-technique.md`, `.umap` du niveau principal. Seul `consistency-keeper` met à jour les documents partagés, en fin de branche, et ne modifie jamais `CLAUDE.md`, `Rendus/` ni un statement IA.
- **Seul `bp-integrator` écrit dans l'éditeur** (MCP Unreal), uniquement pour les étapes [MCP] de la checklist, **un seul à la fois**, après la compilation. Les outils exclus en v1 sont listés dans le workflow (section 10).
- Un agent qui doute le dit au lieu d'inventer ; il ne délègue à aucun autre agent.
