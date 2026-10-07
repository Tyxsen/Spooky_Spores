# CLAUDE.md — Spooky_Spores

Ce fichier transmet tout le contexte du projet à une nouvelle session de Claude. Tu ne connais rien de l'historique : tout ce qui compte est écrit ici. Lis-le entièrement avant toute action. Les points marqués **(à vérifier)** sont ceux dont l'état exact dans le dépôt n'était pas connu au moment de la rédaction : vérifie-les dans le code ou demande à l'utilisateur.

---

## 1. Méthode de travail (le plus important)

### Rôle de Claude

L'utilisateur (Timéo) **écrit le code lui-même**. C'est un choix pédagogique délibéré : il est étudiant et doit comprendre et pouvoir expliquer tout ce qu'il produit (c'est une exigence de son école, voir section 2).

Le fonctionnement établi depuis le début du projet :
1. Claude explique ce qu'il faut faire, pourquoi, et pose les questions de design nécessaires.
2. Timéo réfléchit, écrit le code, et le montre.
3. Claude relit, signale les erreurs en expliquant leur cause, et Timéo corrige.

**Règles qui en découlent :**
- **Ne modifie jamais les fichiers source (C++, Blueprints, config) sans demande explicite.** Par défaut, tu guides, tu ne codes pas à la place de l'utilisateur.
- Quand tu donnes du code, privilégie les signatures, les étapes en pseudo-code et les appels d'API non évidents. Donne du code complet seulement quand l'utilisateur est bloqué et le demande, ou pour des corrections très courtes.
- Tu peux créer ou modifier des fichiers de documentation (README, documentation technique, TODO) quand l'utilisateur le demande.
- Tu peux proposer des mises à jour de CLAUDE.md (nouvelle décision, feature terminée, piège découvert, point « à vérifier » résolu). Présente la modification et applique-la après validation de l'utilisateur.
- **Avant toute action** (modification, commande, génération) : explique ce que tu veux faire et comment.
- Pour les commandes Git ou shell risquées (reset, suppression, force push), explique toujours les conséquences avant.
- **Avant de passer à la phase de test côté éditeur** (Blueprints, assets, PIE) **: review systématique du code C++ écrit entretemps.** Ne pas attendre que Timéo demande la review à chaque fois.

### Style de communication

- Réponds **en français**.
- Sois **didactique** : explique les concepts, ne suppose pas qu'ils sont connus. Explique le « pourquoi », pas seulement le « quoi ».
- Sois **direct et concis** : pas de flatterie, pas de formules creuses.
- **Détaille ton raisonnement** à chaque étape.
- **Challenge l'utilisateur** : ne valide jamais une idée par défaut. Si tu vois une faille, un meilleur chemin, un risque de scope, ou une contradiction avec une décision passée, dis-le clairement et argumente. Un désaccord argumenté vaut mieux qu'un oui complaisant. Mais quand l'utilisateur a un bon argument, reconnais-le (exemple : il a eu raison de garder la vélocité au relâchement d'un objet, voir section 7).
- **Quand tu te trompes, dis-le clairement** et corrige.

### Exactitude

- **Ne donne que des informations vérifiées. Si tu ne sais pas, dis-le.**
- Le projet est sur **Unreal Engine 5.8.x**, une version récente : pour toute API, fonction ou comportement qui aurait pu changer, vérifie dans la documentation officielle d'Epic plutôt que de répondre de mémoire, et signale-le quand tu n'es pas certain.
- Plusieurs erreurs du passé venaient de réponses de mémoire (voir section 11) : la prudence est justifiée.

### Principes de code

- **KISS, DRY, SOLID**, appliqués concrètement (voir section 6).
- Code **modulaire et réutilisable** : on construit des systèmes génériques, puis on multiplie les implémentations.
- **Pas d'optimisation ni d'abstraction prématurée** : on n'ajoute une couche que quand un besoin concret la justifie.
- **Tests rigoureux, pas de complaisance** : un test doit vérifier le comportement réel, y compris les cas limites (rien détecté, objet détruit, valeur à zéro, etc.). « Ça compile » ne veut pas dire « ça marche ».
- **Tester chaque étape avant de passer à la suite**, et faire des tests de non-régression (rejouer les mécaniques existantes) avant chaque fusion.

---

## 2. Contexte pédagogique

- **Formation** : Mastère « Réalisation de Jeux Vidéo, Rendu Temps Réel 3D et Technologies Immersives », Gaming Campus, promotion **GTech 4**. Projet de fin d'études (Capstone Project), **individuel**.
- **Intervenant** : William RAO FERNANDES (PhD en informatique) -> Pour la 1ère semaine seulement.
- **Organisation** : 9 semaines de cours réparties en 3 blocs de 3 semaines. Chaque semaine est une étape du **même** projet, qui évolue progressivement.
  - **Bloc 1** — Gameplay, C++/Blueprint et physique avancée (Chaos). Se termine par un prototype de gameplay physique.
  - **Bloc 2** — Rendu temps réel : matériaux, shaders, Niagara/VFX, éclairage, post-process, profiling et optimisation GPU. Aussi le son et le visuel au sens large.
  - **Bloc 3** — Mise en scène et expérience joueur : caméra, Sequencer, sound design, rythme, narration environnementale, polish final.
- **Rythme** : les semaines 1 et 2 de chaque bloc donnent lieu à un rendu noté ; la semaine 3 à un rendu avec **soutenance orale** en anglais. Les semaines 2 et 3 du bloc 1 seront entièrement en anglais.
- **Alternance** : en dehors des semaines de cours, l'utilisateur est en entreprise et ne travaille sur le projet que le soir et le week-end. Objectif personnel : un jeu jouable de A à Z en mai/juin.
- **Cahiers des charges** : un CDC « fil rouge » pour l'année, et un CDC par semaine, communiqué au début de la semaine concernée. Le CDC de la semaine 2 du Bloc 1 n'était pas encore connu au moment de la rédaction.
- **Principe répété dans tous les CDC** : la qualité technique prime sur la quantité de contenu. Le jeu doit être un bac à sable **systémique** : le joueur expérimente avec les systèmes et déclenche des réactions dans l'environnement.
- **Exigences transversales** : architecture claire et modulaire, systèmes indépendants du Level Blueprint, prototype stable, choix techniques explicables, documentation technique courte à chaque rendu.
- **IA** : l'usage de l'IA est autorisé comme assistance, mais tout élément produit avec elle doit être compris, vérifié, testé et explicable. Un **statement d'intention IA** d'au moins une page est obligatoire à chaque rendu (où, pourquoi, comment l'IA a été utilisée, bénéfices, limites). L'utilisateur rédige lui-même ce statement : ne le rédige pas à sa place.

### Rendu de la semaine 1 (terminé et livré)

Exigences qui étaient notées : Character jouable, système d'interaction générique, objets physiques (masse, gravité, collisions, friction, forces, impulsions), mécaniques Push / Pull / Launch, plusieurs types de collisions et triggers, architecture modulaire, une première expérimentation avec Chaos, au moins un outil de debug, documentation technique, statement IA. Grille sur 150 points (connaissances 60, compétences 60, savoir-être 30).

---

## 3. Le jeu

Présentation complète dans le `README.md` (3C et description). En résumé :

Le joueur est un **nécromancien** qui apparaît dans une cabane abandonnée. Il récolte des ressources, développe des champs, fabrique des potions dans un chaudron, affronte les soldats envoyés par le village voisin, récupère leurs os pour les ressusciter en serviteurs, qui automatisent ensuite sa base. Quête optionnelle de fin : conquérir le village.

**Valeurs du 3C à respecter :**
- Personnage d'environ 1m80. Marche : **400** unités. Course : **650** unités. Endurance : environ 10 secondes de course, recharge après **4 secondes** sans courir.
- Portée d'interaction : 2 à 3 mètres.
- Caméra **première personne**, FOV par défaut 90 (réglable dans les paramètres, à terme).
- Sorts : dégâts modérés selon le niveau, types envisagés aveuglement / gel / feu, mana avec recharge d'environ 5 secondes, 4 emplacements.
- Contrôles prévus dans le 3C : ZQSD, souris, Tab inventaire, E interaction, clic gauche sort, maintien du clic droit pour la roue des sorts, barre de raccourcis 1 à 9, une touche à définir pour une vue de gestion top-down qui servira à organiser sa base.

---

## 4. Environnement technique

- **Moteur** : Unreal Engine **5.8.3** (mis à jour depuis 5.8.2). Le `.uproject` référence `"EngineAssociation": "5.8"`, donc les hotfixes sont compatibles. **Les deux ordinateurs de l'utilisateur doivent être sur le même hotfix.** Le README et la documentation mentionnent encore 5.8.2 **(à mettre à jour)**.
- **IDE** : JetBrains Rider.
- **Deux ordinateurs** : l'utilisateur alterne entre deux machines. Git LFS doit être installé sur chacune (`git lfs install`, une fois par machine, avant le clone).
- **Répartition C++ / Blueprint** : la logique et les systèmes en C++, les Blueprints pour utiliser et configurer ces systèmes, et pour le polish.
- **Base** : template **First Person**. Map de travail : **`Lvl_FirstPerson`**.
- **Module** : `Spooky_Spores`, macro d'export `SPOOKY_SPORES_API`.

---

## 5. Arborescence

### Code source (`Source/Spooky_Spores/`), rangé par domaine fonctionnel

```
Spooky_Spores.h / .cpp / .Build.cs   ← fichiers du module, restent à la racine
CollisionChannels.h                   ← alias des canaux de collision custom
Core/
  Character/    Spooky_SporesCharacter, Spooky_SporesCameraManager
  Controller/   Spooky_SporesPlayerController
  GameMode/     Spooky_SporesGameMode
Interaction/    Grabbable, Interactable, InteractionComponent, GrabComponent, PhysicsGrabbable
Narrative/      StoryTrigger
Resources/      ResourceTypes (header seul), ResourceCounterComponent, ResourceDrop, HarvestableResource
```
Règle : **un nouveau domaine = un nouveau dossier**. On ne crée des sous-dossiers à l'intérieur d'un domaine que quand il dépasse 6 à 8 fichiers.

### Content Browser (`Content/Spooky_Spores/`)

Organisation inspirée du [UE5 Style Guide d'Allar](https://github.com/Allar/ue5-style-guide) :
```
Core/          Character/, Controller/, GameMode/, Input/Actions/   ← briques structurantes
Placeables/    Interactables/ (BP_Rock), Narrative/ (BP_StoryTrigger_Entrance),
               Destructibles/ (GC_Mushroom),
               Resources/ (BP_Drop_Seed, BP_Drop_Stone, BP_Drop_Wood,
                           BP_Harvestable_Plant, BP_Harvestable_Stone, BP_Harvestable_Wood)
UI/Widgets/    WBP_StoryText, WBP_ResourceEntry, WBP_ResourceCounter
Characters/Mannequins/...   ← contenu du template
Maps/
__ExternalActors__/, __ExternalObjects__/   ← voir l'avertissement ci-dessous
```
- `Core/` contient ce qu'on ne modifie pas à la légère ; `Placeables/` contient les instances concrètes, librement ajustables.
- Emplacements confirmés dans le Content Browser : les Blueprints de drops et de nœuds récoltables sont dans `Content/Spooky_Spores/Placeables/Resources/` (voir ci-dessus pour les noms).

**Avertissement : `__ExternalActors__` et `__ExternalObjects__` ne sont pas des assets inutilisés.** Le projet utilise le système **One File Per Actor** : chaque acteur placé dans une map est un fichier séparé dans ces dossiers. Les supprimer viderait les niveaux. Seuls les sous-dossiers correspondant à une map supprimée sont orphelins.

### Documentation (racine du projet)

- `README.md` : présentation du jeu, 3C, lien vers les Releases GitHub.
- `documentation-technique.md` : **document vivant** décrivant l'architecture actuelle, organisé **par système** (Interaction, Récolte, Mouvement, Narration, Chaos, Debug...), jamais par semaine. Réécrit en continu au fil des features, aucune section datée, **pas de statement IA** (ça, c'est le rôle des fichiers `Rendus/`, voir ci-dessous). Restructuré en ce sens le 2026-10-01 (ancien découpage « Semaine 1 » archivé).
- `Rendus/` : un fichier **figé** par rendu noté (ex. `Rendu_Bloc1_S1.md`), jamais remodifié après coup, chacun avec **son propre** statement d'intention IA (écrit par Timéo, jamais par Claude). `Rendu_Bloc1_S1.md` est la version exacte de ce qui a été soumis en semaine 1, avant la restructuration du 2026-10-01.
- `Documentation_Documents/` : captures (PNG) et `Documentation_Documents/Videos/` (GIF). Ce dossier est exclu de Git LFS pour que les images s'affichent sur GitHub. Référencé depuis `documentation-technique.md` (chemin direct) et depuis `Rendus/*.md` (chemin relatif `../Documentation_Documents/...`, un niveau plus bas).
- `TODO.md` : backlog, tenu à jour par l'utilisateur (confirmé présent et à jour le 2026-10-01).

---

## 6. Conventions de code

### Headers et includes

- **Toujours écrire le chemin depuis la racine du module** : `#include "Interaction/Grabbable.h"`, `#include "Resources/ResourceTypes.h"`. Pas d'include court, même pour un fichier du même dossier (Rider en ajoute parfois automatiquement : à corriger).
- Dans un header, **forward declaration** (`class UPrimitiveComponent;`) quand on ne manipule qu'un pointeur ; include complet dans le `.cpp`.
- **Inclure explicitement ce qu'on utilise**, sans compter sur une inclusion indirecte (exemples : `GameFramework/Pawn.h` pour `Cast<APawn>`, `Engine/World.h` pour `SpawnActor`, `TimerManager.h` pour les minuteurs, `DrawDebugHelpers.h` pour le debug, `Components/StaticMeshComponent.h`).

### Spécificateurs d'accès

- **Le plus restrictif par défaut**, élargi seulement si un besoin concret l'exige.
  - `private` : détail d'implémentation.
  - `protected` : nécessaire à une classe enfant (composant racine que les Blueprints enfants configurent, fonctions virtuelles).
  - `public` : l'API que d'autres systèmes appellent. Les `_Implementation` d'interface sont obligatoirement publiques.
- Les callbacks liés avec `AddDynamic` (overlap, hit) sont des `UFUNCTION()` **privées** s'ils ne sont pas virtuels.
- Organisation d'un header : deux groupes séparés par des commentaires `// -----------FUNCTIONS-----------` et `// -----------PROPERTIES-----------` (exactement 11 `-` de chaque côté du mot). Dans chaque groupe, trois blocs `public`, `protected`, `private`, dans cet ordre. Fichier de référence : `ResourceDrop.h`. Pas encore généralisé à tous les fichiers existants (pas urgent), mais à suivre pour tout nouveau fichier ou toute réécriture.

### UPROPERTY

- Composant créé dans le constructeur : `VisibleAnywhere, BlueprintReadOnly` (visible et configurable, mais le pointeur n'est pas réassignable).
- Réglages : `EditAnywhere`, avec une `Category` (format `"Domaine|Sous-partie"`).
- Membre `private` exposé à Blueprint : `meta = (AllowPrivateAccess = "true")`.
- Valeur fixée au runtime et à inspecter en jeu : `VisibleInstanceOnly`.
- Toujours donner une **valeur par défaut explicite** (y compris pour les enums).
- `TObjectPtr<T>` au lieu des pointeurs bruts pour les membres `UPROPERTY` : adoption **progressive, par domaine**, pas une bascule globale d'un coup. Déjà en place dans tout `Resources/` (`ResourceDrop`, `HarvestableResource`, `FieldPlot`). `GrabComponent` et `Spooky_SporesCharacter` (`Interaction/`, `Core/Character/`) restent volontairement en pointeurs bruts, pas de rétro-conversion prévue. Règle simple : ne jamais mélanger les deux styles **à l'intérieur d'un même fichier**.

### Interfaces Unreal (piège important, vérifié sur ce projet)

- Déclaration dans l'interface : uniquement `UFUNCTION(BlueprintNativeEvent, BlueprintCallable, ...)` sur la méthode, **sans** déclarer `_Implementation`.
- Dans UE 5.8, `GENERATED_BODY()` **génère lui-même** une implémentation par défaut de `_Implementation` pour l'interface. Écrire `IMonInterface::MaFonction_Implementation` dans le `.cpp` de l'interface provoque l'erreur `C2084 : la fonction a déjà un corps`. **Les `.cpp` des interfaces restent vides.**
- Dans la classe concrète : `virtual Type MaFonction_Implementation(...) override;` dans le header, et le corps dans le `.cpp` de la classe.
- Pour vérifier qu'un acteur implémente une interface : `Actor->Implements<UMonInterface>()` (version **U**).
- Pour appeler une méthode d'interface depuis l'extérieur : **toujours** `IMonInterface::Execute_MaFonction(Cible, ...)`, jamais `_Implementation` directement.
- Un override doit reprendre exactement la signature de l'interface (type de retour, paramètres, `const`).

### Commentaires

- **Commentaires de code en anglais** (depuis le 2026-10-01 ; les commentaires antérieurs en français ne sont pas à reprendre rétroactivement). Le reste (discussion, documentation, messages de commit) reste en français.

### Nommage

- Préfixes d'assets : `BP_` (Blueprint), `WBP_` (Widget), `GC_` (Geometry Collection), `IA_` (Input Action).
- Les fichiers C++ n'ont pas le préfixe de classe : `GrabComponent.h` contient `UGrabComponent`.
- Fonctions booléennes formulées en question : `IsHolding`, `IsGrabbableTarget`.
- **Ne jamais nommer un paramètre ou une variable locale comme un membre existant** : Unreal traite le masquage (C4458) comme une erreur. `AActor` possède notamment un membre `Instigator`, d'où le paramètre `InteractingActor`.

### Divers

- Racine d'un acteur : `SetRootComponent(...)`, ne jamais redéclarer un membre `RootComponent` (ça masque celui d'`AActor`).
- Une fonction qui renvoie une référence (`TMap::FindOrAdd`) doit être récupérée dans une référence (`int32&`) si on veut modifier la valeur stockée.
- Boucles de lecture sur un tableau de structures : `for (const FMaStruct& X : Tableau)`.
- Les nœuds latents (`Delay`) sont interdits dans les **fonctions** Blueprint : utiliser un **Custom Event** dans l'Event Graph.

---

## 7. Architecture existante

Schéma général exigé par le CDC : **Joueur → Interaction → Objets interactifs → Systèmes physiques → Environnement**. Aucun système ne connaît les détails d'implémentation d'un autre.

### Interfaces (`Interaction/`)

- **`IGrabbable`** : `UPrimitiveComponent* GetGrabbableComponent() const`. « Je peux être manipulé physiquement. » Par défaut renvoie `nullptr`.
- **`IInteractable`** : `void OnInteract(AActor* InteractingActor)`. « J'ai un comportement à déclencher quand on interagit avec moi. »
- Deux interfaces séparées (ségrégation des interfaces) : un objet peut implémenter l'une, l'autre, ou les deux. Un établi ou un champ ne sont pas saisissables ; un rocher ou un bouclier le sont.

### `UInteractionComponent`

Détecte ce que regarde le joueur. **Ne fait que de la détection.**
- Un **seul trace par frame** (`LineTraceSingleByObjectType` sur `COLLISION_INTERACTABLE`), depuis `Pawn->GetActorEyesViewPoint()`, en ignorant le Pawn propriétaire. Longueur : le maximum des deux portées.
- `GrabRange = 400`, `InteractRange = 200` (réglables).
- `CurrentTargetDistance` vaut `TNumericLimits<float>::Max()` quand rien n'est détecté (et non 0, pour qu'une comparaison de distance échoue naturellement sans cible). `ActualTarget` est un `TWeakObjectPtr<AActor>`. Les deux sont privés.
- API : `IsGrabbableTarget(UPrimitiveComponent*& OutComponent)` (BlueprintPure), `IsInteractableTarget()` (BlueprintPure), `TryInteract()` (BlueprintCallable, réutilise `IsInteractableTarget`).
- **Outil de debug** : `DrawDebugLine` non persistante, verte si une cible est détectée, rouge sinon.

### `UGrabComponent`

Télékinésie, autour d'un `UPhysicsHandleComponent`.
- `HoldDistance = 200`, `LaunchImpulseStrength = 1000`.
- `Grab(UPrimitiveComponent*)`, `Release()`, `Launch()` (impulsion en mode vélocité, `bVelChange = true`, pour un lancer indépendant de la masse, appliquée **avant** le relâchement), `IsHolding()` (via `GetGrabbedComponent()`).
- Le tick met à jour la cible avec `SetTargetLocationAndRotation` (et non `UpdateHandleTransform`, qui n'existe pas sous ce nom).
- **Choix de design assumé** : `Release()` conserve la vélocité de l'objet. Un mouvement brusque suivi d'un relâchement projette l'objet, comme dans la réalité. `Launch()` reste le lancer contrôlé.
- **Choix assumé** : pas de raideur ajoutée au physics handle (l'objet ne se recentre pas brutalement à `HoldDistance`), pour préserver l'impression de poids.
- Le calcul de la position cible est dupliqué entre `Grab()` et le tick ; une factorisation dans une fonction privée `GetGrabTargetTransform` avait été recommandée. **Confirmé non faite** (vérifié dans le code le 2026-10-01, voir dette section 13).

### `APhysicsGrabbable` (Blueprint : `BP_Rock`)

Acteur physique qui implémente `IGrabbable`. `MeshComponent` en racine, profil `PhysicsInteractable`, simulation physique activée. **Masse non codée en dur** : calculée par Chaos, ajustable dans les Blueprints enfants. Renommé depuis `APhysicsInteractable` parce qu'il n'implémente pas `IInteractable`.

### `ASpooky_SporesCharacter` (Blueprint : `BP_FirstPersonCharacter`)

- **Override de `GetActorEyesViewPoint`** : renvoie la position et la rotation monde de `FirstPersonCameraComponent`. Le comportement par défaut (`GetActorLocation() + BaseEyeHeight`) ne correspond pas à la caméra, attachée au socket `head` avec un décalage. Tous les traces s'appuient sur cet override.
- **`DoInteractAction`** (touche E, une seule touche contextuelle) : si un objet est tenu, le relâcher ; sinon si la cible est saisissable, la saisir ; sinon `TryInteract()`. C'est le Character qui arbitre entre les deux composants : aucun des deux ne connaît l'autre.
- **`DoLaunchObjects`** (clic droit) : `Launch()`.
- Input Actions : `InteractAction`, `LaunchObjectsAction` (en plus de celles du template).
- Composants ajoutés sur le Blueprint : `UInteractionComponent`, `UGrabComponent`, `UResourceCounterComponent`.
- Le `BeginPlay` du Blueprint crée `WBP_ResourceCounter` et lui transmet le compteur. **Dette connue** : l'UI devra plus tard être gérée par le PlayerController ou une classe HUD.

### Narration (`Narrative/`)

- **`AStoryTrigger`** : `UBoxComponent` en racine avec le profil intégré `Trigger`, `StoryText` (FText éditable), `bTriggerOnce`, `bHasTriggered`, overlap filtré sur les Pawns, et un `BlueprintImplementableEvent OnStoryTriggered()` : la classe C++ ne connaît pas l'UI.
- **`BP_StoryTrigger_Entrance`** : à l'entrée de la cabane, affiche `WBP_StoryText`.
- **`WBP_StoryText`** : widget générique, Custom Event `ShowText(Message)` : texte, `Delay`, `Remove from Parent`.
- `bHasTriggered` n'est pas sauvegardé entre deux lancements (pas encore de système de sauvegarde).

### Chaos

- **`GC_Mushroom`** : champignon (deux cylindres fusionnés) converti en Geometry Collection, fracture Uniform Voronoi (2 morceaux). **Damage Threshold : 5000 / 500 / 50**, réglé sur l'instance placée dans le niveau. Se casse en tombant ou quand on lui lance un objet. Matériau par défaut (placeholder assumé). Les couleurs des morceaux ne s'affichent que dans l'éditeur (outil de debug de la Fracture Mode).

### Récolte de ressources (`Resources/`)

- **`EResourceType`** (`ResourceTypes.h`, header seul) : `Seed`, `Wood`, `Stone`. Ajouter un type = ajouter une valeur ici et une entrée dans `WBP_ResourceCounter`.
- **`UResourceCounterComponent`** : `TMap<EResourceType, int32>`, `AddResource` (avec `FindOrAdd` récupéré en référence), `GetResourceCount` (avec `Find`), et un délégué `FOnResourceChanged(Type, NewCount)` `BlueprintAssignable`, diffusé à chaque ajout. Nommé « Counter » et non « Inventory » : ce n'est pas un inventaire.
- **`AResourceDrop`** : l'objet ramassable au sol.
  - Profil `ResourceDrop`, simulation physique, overlap events et hit events activés.
  - `InitializeDrop(Type, Amount)` ne fait que stocker les données.
  - `BeginPlay` : branche les événements, applique le saut (`FMath::VRandCone` autour de la verticale, `PopStrength = 300`, `PopConeHalfAngle = 35` degrés convertis en radians, impulsion en mode vélocité), puis lance le délai de ramassage.
  - **Délai de ramassage** (`PickupDelay = 0.5`) : pendant ce délai, les overlaps sont ignorés. À la fin, `EnablePickup` vérifie les acteurs qui chevauchent **déjà** le drop (un overlap déjà commencé ne redéclenche pas d'événement). Si le délai vaut 0, activation directe (un `SetTimer` avec une durée nulle ne se lance pas).
  - `TryCollect` renvoie un `bool` pour éviter un double ramassage. Vérifie que le compteur existe (un ennemi est aussi un Pawn).
  - **Atterrissage** : au premier contact avec un sol (normale avec `Z >= MinGroundNormalZ = 0.7`), physique coupée, rotation réduite au seul yaw, puis repositionnement vertical grâce à `MeshComponent->Bounds` pour que la base touche le sol. Le redressement est instantané (à adoucir au Bloc 2). Sur une pente, le drop reste droit.
- **`AHarvestableResource`** : nœud récoltable, implémente `IInteractable`, profil `PhysicsInteractable` (sinon le trace ne le voit pas).
  - `TArray<FResourceDropEntry> Drops`, chaque entrée : `DropClass`, `Type`, `Amount`, `SpawnCount`. `DropSpawnRadius` réglable.
  - `OnInteract_Implementation` : coupe d'abord la collision du nœud, puis pour chaque entrée et chaque exemplaire, calcule une position (décalage aléatoire indépendant sur X et Y, léger décalage vertical), fait apparaître le drop en **spawn différé** (`SpawnActorDeferred`, `InitializeDrop`, puis `FinishSpawning`), passe au suivant avec `continue` en cas d'échec, et détruit le nœud à la fin.
  - Le spawn différé est indispensable : `SpawnActor` exécute `BeginPlay` et les premiers overlaps **avant** de rendre la main, donc un drop apparu dans la capsule du joueur était ramassé avant d'être initialisé (bug d'origine : du bois donnait une graine).
- **UI** : `WBP_ResourceEntry` (une ressource : `ResourceType` et `Label` éditables par instance, fonction `InitializeEntry(Counter)` qui affiche la valeur initiale et se lie à `OnResourceChanged`, avec un filtre sur le type via un nœud **`Equal (Enum)`**), et `WBP_ResourceCounter` (une entrée par type, fonction `InitializeCounter(Counter)`). Le widget reçoit le compteur au lieu d'aller le chercher (injection de dépendance, pour ne pas dépendre de l'ordre d'initialisation du moteur). L'apparence est volontairement brute : le visuel relève du Bloc 2.

---

## 8. Collisions

### Canaux custom (`Project Settings > Engine - Collision`)

| Canal | Type | Réponse par défaut | Rôle |
|---|---|---|---|
| `Interactable` | Object Channel | Block | Tout ce que le trace de `UInteractionComponent` doit détecter |
| `ResourceDrop` | Object Channel | Block | Les drops, pour qu'ils s'ignorent entre eux |
| `Projectile` | Object Channel | Block | Hérité du template |

- `Interactable` est un **Object Channel**, pas un Trace Channel : on le filtre avec `LineTraceSingleByObjectType` et un `FCollisionObjectQueryParams`, pas avec `LineTraceSingleByChannel`.
- Un canal custom n'a pas de nom d'enum dédié en C++ : il occupe un slot `ECC_GameTraceChannelN`. Le numéro se trouve dans `Config/DefaultEngine.ini`. `CollisionChannels.h` définit l'alias `COLLISION_INTERACTABLE`. Ajouter un alias seulement si un canal est utilisé dans le code.
- La **réponse par défaut** d'un nouveau canal décrit comment **les autres presets** réagissent à lui. Mettre `Ignore` sur `ResourceDrop` aurait fait tomber les drops à travers le sol.

### Presets

| Preset | Collision | Object Type | Réponses |
|---|---|---|---|
| `PhysicsInteractable` | Query and Physics | Interactable | Block partout |
| `ResourceDrop` | Query and Physics | ResourceDrop | Block, sauf Pawn en Overlap et ResourceDrop en Ignore |
| `Trigger` (intégré) | — | — | Utilisé par `AStoryTrigger` |

- `Query` couvre les traces et overlaps, `Physics` la simulation. Un preset en `No Collision` désactive tout, physique comprise.
- Résolution entre deux objets : la réponse la plus faible l'emporte. Blocage seulement si les deux bloquent ; aucune interaction dès qu'un des deux ignore ; overlap sinon.
- Overlap et hit exigent aussi d'être activés sur le composant : `SetGenerateOverlapEvents(true)` et `SetNotifyRigidBodyCollision(true)`.

---

## 9. Contrôles actuels

| Action | Touche |
|---|---|
| Déplacement | ZQSD |
| Regarder | Souris |
| Sauter | Espace |
| Relâcher / saisir / interagir (contextuel) | E |
| Lancer l'objet tenu | Clic droit |
| Sprint (maintenu) | Maj gauche |

- Le clic droit est déjà utilisé pour le lancer, et le 3C le réserve à la roue des sorts. Ne lui ajoute pas de nouveau rôle sans en discuter.
- Refonte des contrôles prévue plus tard, quand il y aura des objets équipables : contrôles dépendant de l'objet en main, via les **Input Mapping Contexts** d'Enhanced Input, en mettant le 3C à jour.

---

## 10. Git et workflow

### Git LFS

`.gitattributes` :
```
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap filter=lfs diff=lfs merge=lfs -text
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.psd filter=lfs diff=lfs merge=lfs -text

Documentation_Documents/** -filter -diff -merge -text
```
- La dernière ligne exclut la documentation de LFS (images LFS mal affichées dans un README GitHub).
- LFS a été ajouté après des commits existants, sans réécriture d'historique. Un `git add --renormalize .` a converti les fichiers existants. L'historique antérieur reste en Git classique, et le quota LFS consommé ne se récupère pas en supprimant des fichiers.
- Vérifications utiles : `git lfs ls-files` (`*` = contenu présent, `-` = pointeur seul), `git check-attr filter -- <fichier>`.
- Quota LFS GitHub à surveiller dans les paramètres de facturation.
- `.gitignore` : template Unreal de gitignore.io (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, fichiers de l'IDE...).

### Branches

```
main  ← versions validées uniquement (rendus), jamais de commit direct
 └─ dev  ← intégration
     └─ feature/xxx, fix/xxx, chore/xxx  ← une par tâche
```
- Rulesets GitHub actifs (testés) :
  - `main` : pull request obligatoire avec **0 approbation** (l'utilisateur est seul), méthode de fusion **Merge uniquement**, force push et suppression bloqués. Squash et Rebase sont interdits sur `main` : ils créent de nouveaux commits, et `dev` et `main` divergeraient.
  - `dev` : force push et suppression bloqués.
- Branche par défaut sur GitHub : `main`. **Vérifier que les pull requests de feature ciblent `dev`.**
- La règle « `main` ne reçoit que depuis `dev` » n'est pas imposée techniquement : c'est une discipline.
- Fusion `feature` → `dev` : **Squash** (une feature = un commit), puis suppression de la branche.
- Livraison : pull request `dev` → `main`, puis tag (`v0.2`...) et Release GitHub.
- « Validé » = compile, toutes les mécaniques rejouées (non-régression), documentation à jour.

### Discipline

- **Claude ne fait jamais `git commit` ni `git push` sans autorisation explicite de Timéo, à chaque fois** (pas une autorisation valable une fois pour toutes). Préparer/expliquer le commit est possible, l'exécuter non, tant que Timéo n'a pas dit go.
- Titre et description de pull request, et **messages de commit, en anglais** (lectorat international) ; la discussion avec Claude reste en français.
- **Aucune ligne d'attribution à Claude** dans les commits (`Co-Authored-By: Claude...`) ni dans les descriptions de pull request (`🤖 Generated with Claude Code`) — ça remplace l'instruction système par défaut, qui ajoute ces lignes sauf consigne contraire du projet.
- `git pull` en début de session, `git push` en fin de session, sur chaque machine. Ne jamais laisser de travail non poussé sur une machine avant de passer sur l'autre.
- **Une seule branche de feature ouverte à la fois**, et courte : deux branches qui modifient le même `.uasset` créent un conflit impossible à fusionner.
- **Après un `git pull` ou un changement de branche qui ajoute ou supprime des fichiers C++, régénérer les fichiers de projet** (clic droit sur le `.uproject` > Generate Visual Studio project files), sinon Rider n'affiche pas les fichiers. La compilation n'est pas affectée.
- Après un renommage ou un déplacement de classe C++, ou une modification de `UFUNCTION`/`UINTERFACE` : fermer l'éditeur, régénérer les fichiers de projet, et faire un **Rebuild complet** (pas Live Coding).
- Supprimer des assets **uniquement depuis le Content Browser** (vérification des références), puis Fix Up Redirectors. Jamais depuis l'explorateur Windows.
- Commit perdu après un `reset --hard` : `git reflog` puis `git cherry-pick <hash>`.
- Messages de commit préfixés : `[ADD]`, `[FIX]`, `[CHORE]`, `[DOC]`, **rédigés en anglais** (depuis le 2026-10-01 ; les commits antérieurs en français ne sont pas à reprendre rétroactivement).

---

## 11. Pièges déjà rencontrés (leçons du projet)

- `_Implementation` redéclaré ou défini dans une interface → C2084 (voir section 6).
- Membre `RootComponent` redéclaré → masque celui d'`AActor`, l'acteur n'a pas de racine.
- Paramètre nommé `Instigator` dans un acteur → C4458.
- Variable locale portant le nom d'un `UPROPERTY` → la valeur de l'éditeur est ignorée.
- `FindOrAdd` récupéré par copie → le compteur ne s'incrémentait jamais.
- `FindComponentByClass` appelé sans cible → cherche sur `this`.
- `Destroy()` dans `BeginPlay` → chaque drop se détruisait à sa création.
- Collision désactivée dans le constructeur au lieu du moment de l'interaction → nœud invisible pour le trace.
- `return` au lieu de `continue` dans une boucle de spawn → fin de fonction sautée.
- Ajouter un `float` à un `FVector` → ajouté aux trois composantes.
- `FHitResult.Location` utilisé quand le trace n'a rien touché → vaut `(0,0,0)`.
- `DrawDebugLine` persistante appelée à chaque tick → accumulation de lignes.
- `GetActorEyesViewPoint` par défaut ≠ caméra réelle → trace décalé.
- `Delay` dans une fonction Blueprint → interdit.
- Nœud `==` numérique au lieu de `Equal (Enum)` → impossible de brancher un enum.
- Drops apparus les uns dans les autres, qui se bloquaient entre eux → vitesses d'expulsion énormes (résolu par le canal `ResourceDrop` en Ignore).
- `SpawnActor` exécute `BeginPlay` et les overlaps avant de rendre la main → spawn différé.
- Damage Threshold par défaut trop élevé pour un petit objet.
- Question systématique à se poser : **à quel moment cette ligne s'exécute-t-elle ?**

---

## 12. État actuel et planning

### Où en est le projet

- **Rendu de la semaine 1** : livré (build en Release GitHub, documentation, statement IA).
- **Récolte de ressources (Checkpoint A)** : terminée, testée, fusionnée dans `dev`, branche supprimée. Documentée dans `documentation-technique.md`.
- **Sprint et endurance** : terminé, testé, fusionné dans `dev`, branche supprimée. Documenté dans `documentation-technique.md`.
- **Plantation (Checkpoint B)** : terminée et testée (planter/pousser/récolter, champ réutilisable, debug visuel par couleur). **Fusionné dans `dev`** en Squash (confirmé le 2026-10-06). Documenté dans `documentation-technique.md`. Branche `feature/plantation` supprimée.
- **Chaudron** : terminé et testé (recette 3 Bois + 1 Pierre + 2 Blé → 1 Potion, barre de progression fonctionnelle, potion créditée au compteur de ressources). **Fusionné dans `dev`** en Squash (confirmé le 2026-10-07). Documenté dans `documentation-technique.md`. Branche `feature/cauldron` supprimée.

### Planning jusqu'au Bloc 2

Objectif : la **boucle de jeu principale** jouable de bout en bout, plutôt que le maximum de mécaniques :
> récolter → planter → fabriquer une potion → combattre → récupérer des os → ressusciter un serviteur → le serviteur récolte

| Période | Semaine | Objectif de fin de semaine |
|---|---|---|
| Actuelle | Fin de semaine | Chaudron terminé et fusionné |
| Période 1 | Cours (Bloc 1, S2) | Livrable du CDC (prioritaire) |
| Période 2 | Alternance | Composant de santé + frapper les arbres et pierres + premier sort avec mana |
| | Alternance | Un soldat ennemi simple : se déplace, attaque, meurt, laisse du butin (réutilise les drops) |
| | Cours (Bloc 1, S3) | Livrable du CDC + oral de fin de Bloc 1 |
| Période 3 | Alternance | Résurrection : potion + cadavre = serviteur (le cadavre implémente `IInteractable`) |
| | Alternance | Serviteur qui récolte la ressource la plus proche |
| | Alternance | Semaine tampon : stabilisation, retard, documentation |

**Règles du planning :**
- Les semaines de cours sont réservées au CDC. Quand un CDC arrive, recaler le planning dessus.
- Une mécanique par semaine d'alternance, livrée entière (branche fusionnée dans `dev`). L'utilisateur n'a pas de volume horaire fixe : il finit la tâche de la semaine, puis prend de l'avance sur la suivante s'il a du temps, **seulement après avoir fusionné** la précédente, et sans anticiper le travail des semaines de cours.
- Quand une semaine déborde, **on coupe ou on repousse en fin de file**, on ne décale pas tout le planning.
- Les deux semaines d'IA (ennemi, serviteur) sont les plus optimistes : viser le strict minimum.
- Les sorts arrivent avant le Bloc 2 pour que le Bloc 2 leur donne leurs effets visuels.

### Idée validée pour la période 2 : frapper les ressources

Frapper un arbre ou une pierre plusieurs fois (vibration de l'objet, particules d'impact au Bloc 2) plutôt que de les récolter avec E. Architecture prévue : les arbres et pierres reçoivent le **composant de santé**, et deviennent les premières cibles du système de dégâts, qui servira ensuite aux sorts et aux ennemis. Utiliser le système de dégâts intégré d'Unreal (`UGameplayStatics::ApplyDamage`, événement `OnTakeAnyDamage`) plutôt qu'un système maison. À zéro point de vie, le nœud fait apparaître ses drops avec la logique existante. **Ne pas coder de logique de coups spécifique aux arbres avant ça.** Question de design encore ouverte : avec quoi le nécromancien frappe-t-il (bâton, mains, outils, sort) ? Les champs restent récoltés avec E.

### Repoussé après le Bloc 2

Les trois autres sorts et la roue de sélection, la barre de raccourcis à 9 emplacements et les objets équipables, la refonte des contrôles, la vue de gestion top-down, la construction de base, la conquête du village, le menu de paramètres (son, touches, FOV), un vrai système d'inventaire.

---

## 13. Backlog et dette connue

**Technique**
- Système de sauvegarde : persister `bHasTriggered` des `AStoryTrigger`, l'état des objets déplacés, les ressources, etc. À prévoir avant les mécaniques de progression.
- Déplacer la création de l'UI hors du personnage (PlayerController ou HUD) quand l'interface grossira.
- Les drops au sol ne disparaissent jamais : prévoir une disparition après un délai (sur le modèle du délai de ramassage), surtout avant les serviteurs.
- Push n'est pas une action indépendante : fusionné dans le grab (choix documenté pour le rendu de la semaine 1).
- Un objet tenu peut traverser des obstacles fins si le joueur bouge vite (limite de `UPhysicsHandleComponent`).
- Factoriser le calcul de la cible dans `UGrabComponent` (confirmé non fait).
- Adopter `TObjectPtr` dans toutes les classes (optionnel).
- Nettoyage optionnel des assets inutilisés du template (animations `Pistol`...), sur une branche `chore/`, via le Content Browser, en vérifiant les références (l'Animation Blueprint des bras en utilise certaines).

**Ressenti et visuel (Bloc 2)**
- Son et effet au ramassage d'un drop.
- Matériaux réels (champignon, drops, nœuds), apparence de l'UI.
- Particules d'impact sur les arbres et pierres.
- Plus de fragments sur le champignon si besoin.
- Matériau réel pour `AFieldPlot` (remplace la sphère de debug colorée par état).

**Gameplay à rediscuter**
- Attraction des drops vers le joueur à proximité, fusion des drops identiques proches.
- Repousse des arbres et pierres (à réfléchir avec la plantation).
- Interface de sélection de recette pour le chaudron, pertinente seulement à partir d'au moins deux recettes différentes.
- Idée initiale du chaudron (UI drag-and-drop avec inventaire à droite, cases de recette à gauche) mise de côté pour l'instant : dépend d'un vrai système d'inventaire, repoussé après le Bloc 2 (voir section 12). À reprendre à ce moment-là.

---

## 14. Conseils à l'utilisateur, à rappeler si besoin

- Le dimanche soir, 10 minutes : cocher l'objectif de la semaine et relire le suivant.
- En fin de session, noter en une ligne la prochaine étape dans le `TODO.md`, pour ne pas perdre de temps à la reprise.
- Documenter chaque feature au moment de la fermer, pas au moment du rendu.
- Garder de temps en temps une session « plaisir » (narration, croquis, exploration du niveau) pour entretenir la motivation sur la durée.
