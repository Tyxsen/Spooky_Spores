# Spec — Zone de base

**Item TODO** : 2.2b — Zone de base (`UBoxComponent` qui fournit elle-même ses points)
**Branche** : feature/base-zone
**Statut** : décisions de Timéo intégrées (2026-11-10), prête pour `cpp-implementer` au sens « décisions intégrées » ; la spec complète n'a pas encore été validée par Timéo

## Décisions de Timéo (2026-11-10)

1. **Hauteur** : `bIgnoreHeight = true` par défaut, validé (point tiré au plan médian de la boîte).
2. **Retrouver la zone** : le Character est spawné depuis un `PlayerStart` (la référence `EditInstanceOnly` sur lui est donc impossible). `UBaseZoneSubsystem` **accepté**, avec ses 2 fichiers.
3. **Forme dans le niveau** : Blueprint `BP_BaseZone` (enfant de `ABaseZone`), placé dans le niveau. La boîte **englobe `BP_Fences`** (conforme au `GDD.md` : « volume englobant la clôture ») : posée à la main, approximative, la clôture arrondie tient entièrement dans la boîte, donc tout ce qui est à l'intérieur de la clôture est dans la boîte. La boîte n'est pas couplée à la clôture.
4. **Une seule zone de base par niveau** : validé (une seule base, autour d'un bâtiment).
5. **Pas de marge intérieure (inset)** en 2.2b ; à revoir pendant le test.
6. **Une seule boîte** suffit pour l'instant. Évolution prévue (pas un risque à traiter maintenant) : Timéo repassera sur cette feature pour caler la zone sur la future gestion de base (items 13-14).
7. **Extension de départ** (500, 500, 200) : validée.
8. **Aucun debug visuel en C++.** La boîte est rendue visible en jeu par la propriété standard `Hidden In Game` du composant, réglée sans code de dessin. À remettre à `true` avant Release (réglage de propriété, pas du code à retirer).

> Les points marqués **[CHOIX]** sont des décisions de conception que j'ai prises faute d'arbitrage dans le GDD ou le TODO. Ceux qui ne sont plus marqués « à confirmer » ont été validés ci-dessus.

## 1. Objectif

Donner au jeu le concept technique de « zone de base » (`GDD.md`, section 4, « Dépendance technique à prévoir ») : un volume que les serviteurs interrogent pour savoir « ce point est-il dans la base ? » et « donne-moi un point au hasard dans la base ». Ce volume est la brique dont dépendent l'errance et le retour automatique des serviteurs (item 2.2c) et, plus tard, la vue de gestion et la construction (items 13-14). Cette étape ne change rien de visible pour le joueur (la boîte filaire visible en jeu est un réglage de développement, voir 2.6) : elle pose le contrat que 2.2c consommera.

## 2. Comportement attendu

### 2.1 Forme : acteur dédié `ABaseZone` **[CHOIX]**

`ABaseZone` est un `AActor` dont la racine est un `UBoxComponent` (`ZoneBox`). Un composant seul ne peut pas être posé dans un niveau : il faudrait de toute façon un acteur porteur. Les alternatives écartées :
- **Composant ajouté à `BP_Fences`** : couple la zone à un décor. Or la zone doit survivre à un agrandissement de la base (items 13-14) alors que la clôture n'est qu'un élément visuel. `BP_Fences` n'est pas modifiée.
- **Classe `UBaseZoneComponent : UBoxComponent`** : n'apporte rien de plus qu'un acteur ici, et ajoute un acteur porteur à créer quand même.

Un acteur dédié est aussi le bon objet pour porter une référence (`EditInstanceOnly`) et, plus tard, pour recevoir un redimensionnement (via `SetBoxExtent` ou l'échelle de l'acteur).

Emplacement : `Source/Spooky_Spores/Base/BaseZone.h/.cpp`. Nouveau dossier `Base/` (règle de `CLAUDE.md` section 5 : un nouveau domaine = un nouveau dossier). Il accueillera plus tard la grille de construction (item 14) ; ne pas le ranger dans `Servants/`, car la zone n'est pas une affaire de serviteurs mais de base.

### 2.2 Le calcul se fait dans l'espace local de la boîte

- **Test d'appartenance** : le point monde est ramené dans l'espace local du composant (`GetComponentTransform().InverseTransformPosition`), puis comparé composante par composante à l'extension **non mise à l'échelle** du composant (`GetUnscaledBoxExtent`). Les trois composantes locales doivent être dans `[-Extent, +Extent]` (bornes incluses, avec une petite tolérance, voir 4). L'échelle, y compris non uniforme, est déjà dans la transformation inverse : ne pas la compter une seconde fois.
- **Tirage** : un point est tiré dans l'espace local, uniformément sur chaque axe, dans `[-Extent, +Extent]` (extension non mise à l'échelle), puis transformé en monde par `GetComponentTransform().TransformPosition`. Comme la transformation est affine, la densité reste uniforme dans le volume monde, même avec une échelle non uniforme. Tirer en monde dans une boîte alignée sur les axes serait faux dès que la boîte est tournée (on tirerait dans la boîte englobante, pas dans la boîte).
- **Aucune dimension n'est mémorisée** : chaque appel relit le composant. Si une future construction fait `SetBoxExtent`, `SetActorScale3D` ou déplace l'acteur, l'appel suivant en tient compte. Un serviteur ne stocke ni centre, ni extension, ni limites.

### 2.3 Traitement de la hauteur (Z) **[CHOIX, validé]**

« Hauteur » désigne l'axe Z **local** de la boîte (identique au Z monde tant que la boîte n'est pas inclinée).

Raison du problème : le terrain est une colline (`documentation-technique.md` section 12). Une boîte posée à la main ne suivra jamais exactement le sol, et un serviteur debout a son origine à mi-capsule. Un test 3D strict classerait « hors zone » un serviteur dont le sol dépasse la boîte de quelques centimètres. La notion de « territoire » est d'ailleurs une notion de plan (« à l'intérieur de la clôture »), et le moteur lui-même mesure l'arrivée en 2D (`CLAUDE.md` section 11).

Proposition : un réglage `bIgnoreHeight` (`EditAnywhere`, défaut **`true`**) qui commande les deux fonctions de façon cohérente :

| | `bIgnoreHeight = true` (défaut) | `bIgnoreHeight = false` |
|---|---|---|
| `IsPointInZone` | teste seulement X et Y locaux | teste X, Y et Z locaux |
| `GetRandomPointInZone` | X et Y tirés, **Z local = 0** (plan médian de la boîte) | X, Y, Z tirés |

Invariant : **tout point renvoyé par `GetRandomPointInZone` satisfait `IsPointInZone`**, quelle que soit la valeur de `bIgnoreHeight`.

Conséquence pour 2.2c (à écrire dans sa spec, pas ici) : le point tiré est au plan médian de la boîte, pas au sol. La projection NavMesh devra utiliser une extension verticale qui couvre l'écart entre ce plan et le terrain, sinon elle échouera (piège « Default Query Extent » de `CLAUDE.md` section 11). `GetZoneExtent()` (6) existe pour cela.

### 2.4 Collision de la boîte

`ZoneBox` ne sert qu'à des calculs géométriques, pas à la physique ni aux requêtes :
- Preset de collision `NoCollision` (preset intégré ; **à noter** : avec ce preset, la commande `show collision` ne dessine pas la boîte, voir 2.6). Le trace d'interaction (`Interactable`, `LineTraceSingleByObjectType`) ne la voit donc pas, elle ne bloque pas le joueur ni les drops, et ne génère aucun overlap.
- `SetGenerateOverlapEvents(false)`.
- `SetCanEverAffectNavigation(false)` explicite : la zone ne doit jamais découper le NavMesh dont 2.2c dépend. Avec `NoCollision` elle ne devrait déjà pas le faire, mais l'explicite protège d'un changement de preset en éditeur.
- Pas de canal custom, pas de nouvel alias dans `CollisionChannels.h`.

Contrairement à `AStoryTrigger` (preset `Trigger`, qui réagit aux Pawns), la zone n'a **aucun** événement d'overlap : c'est un point-dans-boîte calculé à la demande, pas un trigger.

### 2.5 Retrouver la zone : sous-système monde **[CHOIX, validé]**

`CLAUDE.md` section 11 recommande une référence explicite `EditInstanceOnly` plutôt que `TActorIterator`. Cette recette **ne s'applique pas ici** :
- Les serviteurs sont **spawnés par le code** (`DoPlaceServant`) : une propriété `EditInstanceOnly` sur `AServant` n'a aucune instance de niveau à laquelle être assignée.
- La référence devrait donc passer par l'appelant, le `Spooky_SporesCharacter`. Or le personnage est **spawné depuis un `PlayerStart`** (confirmé par Timéo), pas placé dans le niveau : le même problème se pose. Un Blueprint ne peut pas pointer vers un acteur de niveau dans ses valeurs par défaut de classe.

Solution proposée : `UBaseZoneSubsystem : UWorldSubsystem` (`Base/BaseZoneSubsystem.h/.cpp`), un registre à une seule entrée.
- `ABaseZone::BeginPlay` s'enregistre ; `ABaseZone::EndPlay` se désenregistre (seulement si c'est bien l'instance enregistrée).
- Une seule zone par monde. Si une seconde s'enregistre : `UE_LOG(LogSpooky_Spores, Error, ...)` qui nomme les deux acteurs, **la première reste enregistrée, la seconde est ignorée**. Le comportement est déterministe, ce qui est précisément ce qui manque à `TActorIterator`.
- Les consommateurs demandent la zone à chaque décision : `GetWorld()->GetSubsystem<UBaseZoneSubsystem>()->GetBaseZone()`. Le sous-système tient un `TWeakObjectPtr<ABaseZone>` : une zone détruite redevient `nullptr` sans pointeur pendant.
- Zéro modification de `Build.cs` : `UWorldSubsystem` est dans le module `Engine`, déjà déclaré.

Décision de Timéo : le sous-système est livré en 2.2b (« comment les serviteurs retrouvent la zone » fait partie du contrat de la zone).

### 2.6 Visibilité de la boîte en jeu : `Hidden In Game`, aucun code de dessin

**Aucun code de debug visuel en C++** (décision 8) : pas de `DrawDebug*`, pas de `Tick`, pas de bouton `CallInEditor`, pas de `bDrawDebug`. `ABaseZone` n'inclut pas `DrawDebugHelpers.h` et `PrimaryActorTick.bCanEverTick = false`.

La boîte est rendue visible par la propriété standard du composant : `Hidden In Game` décoché (`bHiddenInGame = false`).

**Où régler : sur `BP_BaseZone` (composant `ZoneBox`, valeur par défaut du Blueprint), pas dans le constructeur C++ [CHOIX].** Raisons :
- Le C++ garde le défaut du moteur (`bHiddenInGame = true`, posé par le constructeur de `UShapeComponent`) : le code est sûr par défaut, une zone n'est jamais visible par accident dans un build livré.
- Un réglage de développement qu'on doit défaire avant Release vaut mieux comme **valeur de propriété** (une case à recocher dans un asset, repérable dans l'éditeur) que comme ligne de C++ à retrouver et supprimer. C'est aussi la règle du projet : le C++ porte la logique, le Blueprint configure.
- `BP_BaseZone` existe de toute façon (décision 3) : ce réglage ne coûte pas d'asset de plus.
- Le réglage est sur la **classe** du Blueprint, pas sur l'instance du niveau : il ne subit donc pas le piège « propriété modifiée sur l'instance » de `CLAUDE.md` section 11.
- Si `bp-integrator` ne peut pas régler cette propriété avec ses outils MCP : étape [HUMAIN] d'une case à décocher, dans le composant `ZoneBox` de `BP_BaseZone`.

**Vérification dans le code source du moteur** (UE 5.8, installation locale `C:\Program Files\Epic Games\UE_5.8\Engine\Source\Runtime\Engine`, lecture de `UShapeComponent` et `UBoxComponent::CreateSceneProxy`). Je n'ai **pas** lancé de PIE ; ce qui suit vient de la lecture du code.
- `UShapeComponent` met `bHiddenInGame = true` dans son constructeur : c'est ce défaut qui rend la boîte invisible en jeu.
- `UBoxComponent` crée un proxy de rendu qui dessine le fil de fer (`DrawOrientedWireBox`), orienté par la transformation du composant : une boîte tournée ou mise à l'échelle est dessinée telle quelle.
- Condition de dessin (`GetViewRelevance`) : `IsShown(View)` et `(!bDrawOnlyIfSelected || sélectionné)`, **ou** (Show Collision actif et collision activée). `bDrawOnlyIfSelected` est une propriété non éditable que ni `UShapeComponent` ni `UBoxComponent` n'initialisent (valeur nulle) : il ne bloque donc pas le dessin pour notre composant. `IsShown` tient compte de `bHiddenInGame` et `bVisible` ; je n'ai lu que le début de son corps (vérifications « forcé caché » et modes éditeur), pas la fin où figure le test du drapeau de jeu : d'où le « à confirmer en PIE ».
- Je n'ai trouvé **aucune** condition de type build non-Shipping dans ce proxy : le dessin n'est pas retiré par un `#if` de ce code. C'est pourquoi le rappel « remettre à `true` avant Release » est nécessaire.
- **Piège** : le chemin `Show > Collision` ne dessine que si la collision est activée sur le composant. Notre boîte étant en `NoCollision` (2.4), la commande console `show collision` ne la montrera **pas**. Le chemin à utiliser est `Hidden In Game` décoché.
- Conclusion : d'après le code, boîte visible en PIE avec `bHiddenInGame = false` et `bVisible = true`. **À confirmer en PIE** (critère G1). Si elle ne s'affiche pas, Timéo investigue lui-même : la spec ne prévoit aucun contournement du rendu.

**Avant Release** : remettre `Hidden In Game` à `true` sur `BP_BaseZone`. Rappel à proposer pour la dette technique de `TODO.md` (écriture par `consistency-keeper`), sous forme de **réglage de propriété** :
> Remettre `Hidden In Game` à `true` sur le composant `ZoneBox` de `BP_BaseZone` avant la première Release (la boîte de la zone de base est visible en jeu pendant le développement).

**Vérifier le tirage** : la vérification est celle des critères statistiques R1-R7 (Automation Spec). Aucun outil visuel annexe n'est prévu (décision 8).

## 3. Critères d'acceptation

Convention : `Z` désigne une `ABaseZone` (ou ses fonctions statiques, voir 6) avec la boîte et la transformation indiquées. Les points sont en unités Unreal (cm). Chaque ligne doit pouvoir **échouer** si le comportement est cassé ; la colonne de droite dit comment.

### Appartenance (`IsPointInZone`)

- [ ] **A1 — boîte axe-alignée.** Boîte centre (0,0,0), extension (500,300,100), `bIgnoreHeight=false`. Dedans : (0,0,0), (499,299,99), (-499,-299,-99). Dehors : (501,0,0), (0,301,0), (0,0,101), (-501,0,0). *Échoue si un axe est inversé ou si l'extension est prise pour des dimensions complètes (le test (501,0,0) passerait alors).*
- [ ] **A2 — bornes.** Un point exactement sur la face (500,0,0) est dedans. Un point à 0,5 cm hors de la face est dehors. La tolérance (4) reste inférieure à 0,5 cm.
- [ ] **A3 — boîte tournée de 90° en yaw.** Centre (1000,0,0), extension locale (500,200,100), yaw 90°, `bIgnoreHeight=false`. Dedans : (1000,400,0). Dehors : (1400,0,0). *Ce second point serait dedans si la rotation était ignorée : le test échoue sur une implémentation qui travaille en monde.*
- [ ] **A4 — boîte tournée de 45° en yaw.** Même boîte, yaw 45°. Un point à 400 cm du centre le long de la direction (cos45°, sin45°, 0) est dedans ; à 600 cm, dehors ; le point centre + (450,0,0) est dehors (en local : x≈318, y≈-318, |y| > 200).
- [ ] **A5 — boîte inclinée (pitch 30°).** Centre (0,0,0), extension (500,300,100), pitch 30°, `bIgnoreHeight=false`. Un point calculé en transformant `(0,0,90)` local en monde est dedans ; `(0,0,110)` local est dehors. Valide l'espace local en 3D.
- [ ] **A6 — échelle non uniforme.** Extension (100,100,100), échelle (3,1,1), `bIgnoreHeight=false`. Dedans : (250,0,0). Dehors : (0,250,0), (350,0,0). *Échoue si l'échelle est ignorée ou comptée deux fois.*
- [ ] **A7 — hauteur ignorée.** Boîte A1, point (0,0,5000). `bIgnoreHeight=true` : dedans. `bIgnoreHeight=false` : dehors. Un point hors du plan XY (501,0,5000) est dehors dans les deux modes.
- [ ] **A8 — suivi du redimensionnement.** Après un premier appel à `IsPointInZone((600,0,0))` qui renvoie faux, `SetBoxExtent((700,300,100))` à l'exécution, le même appel renvoie vrai. *Échoue si les dimensions ont été mémorisées.* Même test avec `SetWorldScale3D` et avec un déplacement de l'acteur.
- [ ] **A10 — échelle négative.** Extension (100,100,100), échelle (-3,1,1), `bIgnoreHeight=false` : (250,0,0) dedans, (350,0,0) dehors, (0,250,0) dehors ; `GetZoneExtent()` renvoie (300,100,100) (composantes positives). *Échoue si l'implémentation utilise `GetScaledBoxExtent()` brut, qui renvoie -300.*
- [ ] **A9 — entrée invalide.** Un point contenant NaN ou une valeur infinie renvoie faux, sans crash ni assertion.

### Tirage (`GetRandomPointInZone`)

- [ ] **R1 — invariant.** Sur 2000 tirages pour chacune des configurations A1, A3, A4, A5, A6 et pour les deux valeurs de `bIgnoreHeight` : la fonction renvoie vrai et chaque point satisfait `IsPointInZone`. *Cible directement la tolérance de bord (4) : un tirage qui retombe pile sur la face doit rester dedans.*
- [ ] **R2 — uniformité grossière (boîte A1, `bIgnoreHeight=false`, 2000 tirages).** Sur chaque axe local, au moins un point dans chaque dixième de l'intervalle `[-E, E]`, et moyenne locale par axe à moins de 10 % de `E` du centre. (Probabilité de faux échec pour un dixième vide : 0,9^2000, négligeable ; pour la moyenne, plus de 10 écarts-types.) *Échoue si le tirage est biaisé vers un coin, ou si l'on tire dans la boîte englobante au lieu de la boîte tournée.*
- [ ] **R3 — espace local.** Configuration A3 (yaw 90°). Sur 2000 tirages, la plage des X monde dépasse 80 % de `[1000-200, 1000+200]` et reste incluse dans `[800, 1200]` (plus tolérance), la plage des Y monde dépasse 80 % de `[-500, 500]`. *Échoue si le tirage ignore la rotation.*
- [ ] **R4 — échelle non uniforme.** Configuration A6. Les X monde tirés atteignent au-delà de 270 (extension monde 300) et restent dans `[-300, 300]` ; les Y monde restent dans `[-100, 100]`.
- [ ] **R5 — hauteur.** `bIgnoreHeight=true` : le Z **local** (point ramené dans l'espace local) de chaque point vaut 0 à 0,01 cm près. Pour une boîte non inclinée, son Z monde vaut le Z du centre. `bIgnoreHeight=false` : les Z locaux couvrent au moins 80 % de `[-E_z, E_z]`.
- [ ] **R6 — non déterministe.** Deux séries de 100 tirages successifs ne sont pas identiques (le tirage n'est pas figé sur une graine).
- [ ] **R7 — pas de mémorisation.** Après `SetBoxExtent` à l'exécution, les tirages suivants couvrent la nouvelle extension (par exemple, X local max > ancienne extension).

### Zone dégénérée (voir 4)

- [ ] **D1.** Extension (0,300,100) : `IsPointInZone` renvoie faux pour tout point, y compris le centre exact ; `GetRandomPointInZone` renvoie faux ; `IsZoneValid()` renvoie faux. Idem pour une échelle (1,0,1), et pour une extension négative saisie à la main (comportement défini : zone invalide, pas de crash). *Le cas « zone d'extension nulle qui accepte son centre » est l'erreur classique d'un `InverseTransform` à échelle nulle : ce test la détecte.*
- [ ] **D2.** Sur une zone invalide, `OutPoint` est laissé à une valeur explicitement définie (voir 6 : `FVector::ZeroVector`) mais l'appelant ne doit pas l'utiliser : le test vérifie le **retour faux**, pas la valeur.

### Collision et construction (testables sans monde sur le CDO, `GetDefault<ABaseZone>()`)

- [ ] **C1.** `ZoneBox` n'est pas nul ; `GetCollisionEnabled() == ECollisionEnabled::NoCollision` ; `GetGenerateOverlapEvents() == false` ; `CanEverAffectNavigation() == false`.
- [ ] **C2.** `ZoneBox` est la racine de l'acteur (`GetRootComponent() == ZoneBox`).
- [ ] **C3.** Valeurs par défaut : `bIgnoreHeight == true`, extension par défaut de la boîte égale à (500, 500, 200) (valeur de la section 5), `PrimaryActorTick.bCanEverTick == false`, et `ZoneBox->bHiddenInGame == true` sur le CDO de `ABaseZone` (le défaut moteur n'est pas modifié par le C++ ; c'est `BP_BaseZone` qui le décoche, voir 2.6). *Ce dernier point échoue si quelqu'un met `SetHiddenInGame(false)` dans le constructeur.*
- [ ] **C4 — pas vue par le trace d'interaction** (monde requis, voir plus bas) : un `LineTraceSingleByObjectType` sur l'objet `COLLISION_INTERACTABLE` traversant la zone ne renvoie pas `ABaseZone`. Un trace sur `ECC_Visibility` non plus, et un `Pawn` placé à l'intérieur n'est pas bloqué.

### Sous-système (monde requis)

- [ ] **S1.** Après `BeginPlay` d'une zone unique, `GetBaseZone()` renvoie cette zone.
- [ ] **S2.** Sans zone dans le monde, `GetBaseZone()` renvoie `nullptr` (sans crash, sans log d'erreur).
- [ ] **S3.** Deux zones : `GetBaseZone()` renvoie **la première enregistrée** et un message `Error` est émis. Détruire la seconde ne désenregistre pas la première. Détruire la première puis lire renvoie `nullptr` (le second reste ignoré : c'est une erreur de level design, pas un cas à rattraper).
- [ ] **S4.** Après destruction de l'unique zone, `GetBaseZone()` renvoie `nullptr` (pas de pointeur pendant).

### Visibilité (vérification humaine, pas de code)

- [ ] **G1 — PIE.** `BP_BaseZone` placé dans `Lvl_Principal`, `Hidden In Game` décoché sur son composant `ZoneBox` : la boîte filaire est visible en jeu, englobant `BP_Fences` (la clôture tient entièrement dans la boîte). Recocher `Hidden In Game` la fait disparaître. C'est la confirmation de la lecture du code moteur de 2.6 ; si elle échoue, Timéo investigue lui-même (aucun repli prévu).
- [ ] **G2 — instance.** Sur l'instance placée, la propriété `Hidden In Game` n'est pas surchargée à la main (sinon elle ignore la valeur du Blueprint, voir `CLAUDE.md` section 11).

Note pour `qa-test-writer` : A1 à A9, R1 à R7 et D1 à D2 portent sur de la logique pure ; les fonctions statiques de 6 permettent de les écrire en **Automation Spec sans monde** (stratégie de test de `docs/WORKFLOW_AGENTS.md` section 7). Les critères C1 à C3 s'écrivent sur le CDO. Ce qui exige un monde (C4, S1 à S4, et au moins un test d'intégration qui passe par l'acteur réel, pour ne pas tester seulement les fonctions statiques) : voir si un monde de test est réalisable en Automation Spec ; si ça ne l'est pas, c'est un Functional Test (map créée par l'humain) ou une vérification manuelle en PIE, à signaler. Un monde de test s'est révélé réalisable en Automation Spec (`BaseZoneWorldSpec`). Le montage de l'infrastructure de test n'a pas modifié `Spooky_Spores.Build.cs`.

## 4. Cas limites

- **Extension nulle ou négative sur un axe, ou échelle nulle sur un axe** : la zone est **invalide**. `IsZoneValid()` renvoie faux, `IsPointInZone` renvoie faux, `GetRandomPointInZone` renvoie faux. Un `Warning` est émis dans `LogSpooky_Spores`, **une seule fois par zone** (drapeau interne), pour ne pas inonder le log (les serviteurs interrogent à chaque décision). Raison : avec une échelle nulle, `InverseTransformPosition` utilise un inverse « sûr » (0) et ramènerait n'importe quel point au centre local, donc **dedans** : il faut tester la validité explicitement avant.
- **Seuil de validité** : chaque composante de l'extension non mise à l'échelle et de la valeur absolue de l'échelle doit être supérieure à `KINDA_SMALL_NUMBER`.
- **Échelle négative** (miroir) : la zone reste valide, le test en local ne dépend pas du signe. **Vérifié dans le code moteur 5.8** : `GetScaledBoxExtent()` vaut `BoxExtent * GetComponentTransform().GetScale3D()` **sans valeur absolue** ; avec une échelle négative il renvoie des composantes négatives. L'implémentation ne doit donc pas s'y fier : elle lit `GetScale3D().GetAbs()` elle-même, et `GetZoneExtent()` renvoie toujours des composantes positives (critère A10).
- **Échelle non uniforme avec boîte tournée** : correcte tant que `ZoneBox` est la racine sans parent (pas de cisaillement possible). Ne pas attacher la zone à un acteur parent mis à l'échelle non uniformément : hors périmètre, à ne pas supporter.
- **Boîte inclinée (pitch/roll)** : supportée par construction (espace local). `bIgnoreHeight` s'applique alors à l'axe Z local, pas au Z monde.
- **Point sur la frontière** : dedans (borne incluse), avec une tolérance interne de **0,1 cm** en espace local non mis à l'échelle, pour que la garantie « un point tiré est toujours dans la zone » (R1) ne dépende pas des arrondis flottants. Cette tolérance doit rester sous 0,5 cm (A2).
- **Zone absente** (aucune `ABaseZone` dans le niveau) : `GetBaseZone()` renvoie `nullptr`. Le consommateur (2.2c) gère ce cas ; la zone n'a rien à faire.
- **Zone pas encore enregistrée** : un serviteur placé dans le niveau peut faire son `BeginPlay` avant celui de la zone (ordre non garanti entre acteurs). Les consommateurs interrogent le sous-système à chaque décision, pas une fois dans leur `BeginPlay`, et ne mettent jamais un `nullptr` en cache.
- **Deux zones dans le niveau** : voir S3. C'est une erreur de level design signalée par un log `Error`, pas un cas géré fonctionnellement.
- **Zone supprimée en jeu** : `TWeakObjectPtr` donne `nullptr` ; aucun pointeur pendant.
- **Zone déplacée ou redimensionnée à l'exécution** : prise en compte au prochain appel (A8).
- **Point tiré hors du sol / dans un obstacle (arbre, clôture, mur)** : normal, la zone ne connaît ni le terrain ni le NavMesh. La boîte englobant le tracé de la clôture, un point tiré peut tomber sur la clôture elle-même ou sur un arbre : c'est traité en 2.2c (projection NavMesh, essais bornés), **pas** en 2.2b.
- **Pas de `World` (CDO, aperçu d'éditeur)** : les fonctions de calcul n'ont pas besoin de `UWorld`.
- **Zone invisible en jeu** : sans effet sur le calcul. `Hidden In Game` ne concerne que le rendu (2.6).

## 5. Paramètres d'équilibrage

Réglages de l'instance placée (ce sont des réglages de level design, pas d'équilibrage de gameplay).

| Paramètre | Défaut | Rôle |
|---|---|---|
| `ZoneBox` extension (`BoxExtent`) | (500, 500, 200) **[CHOIX arbitraire]** | Demi-dimensions. Valeur de départ seulement : la vraie est réglée **à la main pour englober `BP_Fences`** après placement |
| `bIgnoreHeight` | `true` | Voir 2.3 |
| `ZoneBox` > `Hidden In Game` | `false` sur `BP_BaseZone` (`true` dans le C++, défaut moteur) | Boîte filaire visible en jeu pendant le développement ; **à remettre à `true` avant Release** (2.6) |
| Tolérance de bord | 0,1 cm (constante du `.cpp`, non exposée) | Voir 4 |

Pas de valeur de gameplay (vitesse, délai) ici : elles appartiennent à 2.2c.

Piège à vérifier (`CLAUDE.md` section 11) : après avoir réglé l'extension à la main sur l'instance placée, un changement de défaut dans le C++ ou le Blueprint ne s'y propagera plus sans `Reset to Default`.

## 6. API exposée aux Blueprints

Signatures attendues. Le testeur peut s'en servir sans lire l'implémentation.

```cpp
// Base/BaseZone.h
class SPOOKY_SPORES_API ABaseZone : public AActor
{
    GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
    ABaseZone();

    // True if WorldPoint is inside the zone. False for an invalid zone or a non-finite point.
    UFUNCTION(BlueprintPure, Category = "BaseZone")
    bool IsPointInZone(const FVector& WorldPoint) const;

    // Picks a uniformly distributed random point in the zone (drawn in the box's local space,
    // then transformed to world space). Returns false and leaves OutPoint at ZeroVector
    // when the zone is invalid. Never use OutPoint when the return value is false.
    // BlueprintPure = false: UHT would otherwise force a const function with an output param to be pure.
    UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "BaseZone")
    bool GetRandomPointInZone(FVector& OutPoint) const;

    // False if any extent or scale component is <= KINDA_SMALL_NUMBER.
    UFUNCTION(BlueprintPure, Category = "BaseZone")
    bool IsZoneValid() const;

    // Half-sizes of the zone along its local axes, scale included, always positive (absolute value of the scale).
    // 2.2c will need Z to size the vertical extent of its NavMesh projection.
    UFUNCTION(BlueprintPure, Category = "BaseZone")
    FVector GetZoneExtent() const;

    UBoxComponent* GetZoneBox() const { return ZoneBox; }

    // Pure maths, no UObject needed: used by the member functions and callable from Automation Specs.
    // Extent is the UNSCALED local half-extent. Transform is the component's world transform (scale included).
    static bool IsPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
                                     const FVector& WorldPoint, bool bIgnoreHeight);
    static bool TryGetRandomPointInOrientedBox(const FTransform& BoxTransform, const FVector& UnscaledExtent,
                                               bool bIgnoreHeight, FVector& OutPoint);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

// -----------PROPERTIES-----------
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseZone")
    bool bIgnoreHeight = true;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BaseZone", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> ZoneBox;
};
```

Remarques :
- `GetRandomPointInZone` est `BlueprintCallable` avec `BlueprintPure = false` explicite : un nœud pur serait réévalué à chaque branchement de sortie, et donnerait deux points différents pour une même « décision ». Le `BlueprintPure = false` est nécessaire car UHT rend sinon pure toute fonction `const` à paramètre de sortie, même déclarée `BlueprintCallable`.
- Pas de `FVector` retourné directement sur échec : un `(0,0,0)` muet est le piège connu de `CLAUDE.md` section 11 (`FHitResult.Location` quand rien n'est touché).
- Les noms exacts des fonctions statiques sont à discrétion de `cpp-implementer`, mais elles doivent exister, être publiques et ne dépendre d'aucun `UWorld`. Si elles ne sont pas retenues, tous les critères A/R/D devront passer par un acteur spawné et le coût de test augmente.
- Pas de `Tick`, pas d'include de `DrawDebugHelpers.h`, aucun dessin : la visibilité en jeu est un réglage de propriété (2.6).
- Le style des membres suit `CLAUDE.md` : `TObjectPtr` partout dans ce nouveau fichier (ne pas mélanger avec des pointeurs bruts), en-tête organisé avec les séparateurs `// -----------FUNCTIONS-----------` / `// -----------PROPERTIES-----------`, includes depuis la racine du module (`Base/BaseZone.h`), commentaires en anglais.

```cpp
// Base/BaseZoneSubsystem.h
UCLASS()
class SPOOKY_SPORES_API UBaseZoneSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

// -----------FUNCTIONS-----------
public:
    // Returns the registered zone, or nullptr when none is registered (yet).
    UFUNCTION(BlueprintPure, Category = "BaseZone")
    ABaseZone* GetBaseZone() const;

    // Called by ABaseZone::BeginPlay / EndPlay only.
    void RegisterZone(ABaseZone* Zone);
    void UnregisterZone(ABaseZone* Zone);

// -----------PROPERTIES-----------
private:
    TWeakObjectPtr<ABaseZone> RegisteredZone;
};
```

### API que 2.2c consommera (non implémentée ici)

2.2c fera, à chaque décision d'un serviteur :
1. `UBaseZoneSubsystem::GetBaseZone()` ; si `nullptr`, ne rien faire (pas de zone, pas d'errance) et ne pas mettre ce résultat en cache ;
2. **retour hors zone** : `IsPointInZone(Servant->GetActorLocation())` ; faux → la prochaine destination est un point de la zone (aucun mode dédié, cf. TODO 2.2c) ;
3. **errance** : `GetRandomPointInZone(OutPoint)` ; faux → pas de destination ; vrai → projection NavMesh et essais bornés (côté 2.2c) ;
4. `GetZoneExtent().Z` pour dimensionner l'extension verticale de la projection.

Rien d'autre n'est nécessaire côté zone. En particulier, la zone ne fournit **pas** de marge intérieure (inset) : si 2.2c veut éviter les points collés à la clôture, ce sera à revoir pendant le test (décision 5).

## 7. Périmètre de fichiers

**À créer (C++)**
- `Source/Spooky_Spores/Base/BaseZone.h`
- `Source/Spooky_Spores/Base/BaseZone.cpp`
- `Source/Spooky_Spores/Base/BaseZoneSubsystem.h`
- `Source/Spooky_Spores/Base/BaseZoneSubsystem.cpp`

**À créer (tests)** : l'emplacement est à fixer par `qa-test-writer` (aucun test n'existe encore dans le projet, `docs/WORKFLOW_AGENTS.md` section 7). Hors `Source/Spooky_Spores/Base/`.

**À créer (éditeur, par `bp-integrator`, [MCP] possible)**
- `Content/Spooky_Spores/Placeables/Base/BP_BaseZone.uasset` : Blueprint enfant de `ABaseZone`, sans logique, pour respecter la convention `BP_` des placeables. Décision de Timéo (3) : Blueprint, par cohérence avec `BP_Fences`. **Réglage à faire dans ce Blueprint : composant `ZoneBox` > `Hidden In Game` décoché** (2.6) ; `bIgnoreHeight` et l'extension (500, 500, 200) y sont aussi les valeurs par défaut.
- Instance de `BP_BaseZone` placée dans `Lvl_Principal`, **extension et position réglées à la main pour englober `BP_Fences`** (boîte approximative, non couplée à la clôture). Étape [HUMAIN] ou [MCP] selon l'outillage disponible (placement et réglage de propriété d'une instance : réalisable avec des outils limités ; on vérifie à l'œil en PIE, grâce à la boîte visible (`Hidden In Game` décoché), que la clôture arrondie tient dans la boîte ; une boîte approximative suffit).

**Fichiers existants modifiés** : aucun. Notamment, **ni `AServant`, ni `Spooky_SporesCharacter`, ni `BP_Fences`** ne sont modifiés par 2.2b.

**Fichiers points chauds touchés**
- `.umap` du niveau principal (`Lvl_Principal`) : placement de l'instance. **À sérialiser.**
- `Spooky_Spores.Build.cs` : **non requis par la fonctionnalité** (`UBoxComponent` et `UWorldSubsystem` sont dans `Engine`, déjà déclaré). Non modifié par l'introduction de l'infrastructure de test (vérifié : aucune modification de ce fichier dans la branche).
- `TODO.md` (dette technique proposée en 2.6 sous forme de réglage de propriété, et coche de 2.2b), `documentation-technique.md` (nouvelle section « Zone de base », et mention de `Base/` dans la liste de la section 2 qui sera en retard) : écrits **uniquement par `consistency-keeper`** en fin de branche.
- `EResourceType`, `Default*.ini`, `CollisionChannels.h`, `CLAUDE.md` : non touchés.

## 8. Dépendances

- **Aucune dépendance bloquante** : 2.2a est fusionnée (`AServant` existe), mais 2.2b n'en dépend pas.
- **Relation avec `BP_Fences`** : aucune dépendance de code ni de composant. La zone englobe la clôture par réglage manuel de sa position et de son extension ; `BP_Fences` n'est pas modifiée.
- **Bloque 2.2c** (errance), qui consomme l'API de 6.
- **Prépare les items 13-14** (vue de gestion, construction) : la zone doit pouvoir être redimensionnée à l'exécution sans que ses consommateurs le sachent. **Évolution prévue** (décision 6) : Timéo repassera sur cette feature pour caler la zone sur la future gestion de base ; une seule boîte suffit d'ici là. Ce n'est pas un risque à traiter en 2.2b.
- Dépend de `LogSpooky_Spores` (existe, `Spooky_Spores.h`).

## 9. Hors périmètre

- Errance, choix et mouvement vers une destination, `MoveTo`, `AIController` du serviteur, animation : 2.2c.
- Projection NavMesh, essais bornés, retour d'un serviteur posé hors zone, cas du serviteur sans chemin vers la base : 2.2c.
- Marge intérieure (inset) pour éviter les bords : décision 5, à revoir pendant le test.
- Modification d'`AServant`, du Character, de `BP_Fences`, du GDD, du TODO.
- Redimensionnement dynamique de la zone par le joueur, grille de construction, vue top-down : items 13-14. Seul le **suivi** d'un redimensionnement fait par du code existe ici (A8).
- Zone non rectangulaire (plusieurs boîtes, polygone) : évolution possible avec les items 13-14 (décision 6).
- Tout dessin de debug en C++ (décision 8).
- Sauvegarde de la zone (système de sauvegarde, TODO) : l'acteur est placé dans le niveau, sa forme est dans la map.
- Utilisation de la zone par les ennemis ou pour les cases verrouillées du chemin (pilier B du GDD) : non demandé.

## Questions ouvertes (pour Timéo)

Aucune : toutes les questions sont tranchées (voir « Décisions de Timéo » et les validations du 2026-11-10 sur `Hidden In Game` réglé sur `BP_BaseZone`, la ligne de dette technique, et l'absence de repli de rendu).

## Vérifications non faites (à ne pas prendre pour acquises)

- Si `UBoxComponent` se dessine réellement en PIE avec `Hidden In Game` décoché : conclu par lecture du code moteur 5.8, **pas** par un PIE (critère G1). Le corps complet de `FPrimitiveSceneProxy::IsShown` n'a pas été lu en entier.
- Ce qu'un monde de test d'Automation Spec permet sous 5.8.3.
- Que le preset `NoCollision` intégré porte bien ce nom dans ce projet (c'est le nom standard d'Unreal ; à confirmer dans Project Settings).
