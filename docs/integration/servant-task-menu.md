# Intégration éditeur : menu d'assignation des serviteurs (2.1)
Spec : `docs/specs/servant-task-menu.md`. **1 seule étape [MCP] (n° 3), aucun composant hérité du C++ : ne pas lancer `bp-integrator`, Timéo fait tout à la main (plus rapide).**
Éditeur non joignable pendant la planification : chemins vérifiés sur le disque (`Content/`), pas dans l'éditeur.

## Pré-requis
- Branche `feature/servant-task-menu`, arbre propre. Après compilation, l'éditeur doit montrer : `ServantTaskMenuWidget` (Abstract), `ServantSubsystem`, l'enum `EServantTask`, et sur `Spooky_SporesCharacter` la propriété `Toggle Servant Menu Action` (Input) et l'événement `On Servant Menu Toggled`.
- Assets existants : `/Game/Spooky_Spores/Core/Input/IMC_Default`, `/Game/Spooky_Spores/Core/Input/Actions/` (10 `IA_`), `/Game/Spooky_Spores/Core/Character/BP_FirstPersonCharacter`, `/Game/Spooky_Spores/Placeables/Servants/BP_Servant`, `/Game/Spooky_Spores/UI/Widgets/WBP_ControlsPanel`.
- **Assets partagés modifiés : `IMC_Default` et `BP_FirstPersonCharacter`** (aucune autre branche ne doit les toucher en parallèle). **`Lvl_Principal.umap` n'est pas modifié** : s'il apparaît dans `git status`, ne pas le commiter.

## Compilation et rechargement (avant tout)
1. Fermer l'éditeur. 2. Clic droit sur `Spooky_Spores.uproject` > *Generate Visual Studio project files* (fichiers C++ ajoutés). 3. Rider : **Rebuild** complet (nouvelles `UFUNCTION`, pas de Live Coding). 4. Rouvrir l'éditeur sur la branche, lancer les Automation Specs (filtre `SpookySpores`) : tout doit être vert.

## Étapes
1. [HUMAIN] `/Game/Spooky_Spores/Core/Input/Actions/` > clic droit > *Input > Input Action* > `IA_ToggleServantMenu`. Value Type = `Digital (bool)`, **`Trigger when Paused` coché**, ni trigger ni modifier. Attendu : case cochée après sauvegarde. (Pas en [MCP] : aucun outil n'écrit la propriété d'un Data Asset.)
2. [HUMAIN] `IMC_Default` > *Mappings* > `+` : `IA_ToggleServantMenu`, touche `Tab`, sans trigger. Vérifier d'abord que Tab n'est mappée nulle part et qu'aucune autre `IA_` n'a `Trigger when Paused` (un scan binaire n'en a trouvé aucune). Attendu : une seule ligne Tab. (Pas en [MCP] : aucun outil n'édite les mappings.)
3. [MCP] `BP_FirstPersonCharacter`, Class Defaults : `ToggleServantMenuAction` = `/Game/Spooky_Spores/Core/Input/Actions/IA_ToggleServantMenu`. Attendu : valeur relue non vide.
4. [HUMAIN] `/Game/Spooky_Spores/UI/Widgets/WBP_ServantEntry` (*User Interface > Widget Blueprint*, parent `UserWidget`). Designer : racine `Button` `EntryButton` (**`Is Focusable` décoché**) contenant un `TextBlock` `EntryText` ; les deux en *Is Variable*. Variables en **Instance Editable + Expose on Spawn** : `Servant` (réf. objet `Servant`), `ServantNumber` (Integer, 0), `OwnerMenu` (réf. objet `Servant Task Menu Widget`, classe C++ : pas de dépendance circulaire entre les deux WBP).
5. [HUMAIN] Graph de `WBP_ServantEntry` : `Event Construct` → `Format Text` (`Serviteur {N}`, N = `ServantNumber`) → `EntryText > Set Text`. `EntryButton` > *On Clicked* (`+`) → `Is Valid (OwnerMenu)` → `Select Servant` (cible `OwnerMenu`, `Servant`). Fonction `SetHighlighted(IsHighlighted: bool)` → `Select` (jaune / blanc) → `EntryButton > Set Background Color`.
6. [HUMAIN] `/Game/Spooky_Spores/UI/Widgets/WBP_ServantTaskMenu` : Widget Blueprint, *Pick Parent Class* > `ServantTaskMenuWidget`. Designer : `Canvas Panel` > `Border` plein écran (ancres étirées, fond noir alpha 0,6 : il absorbe les clics hors boutons) > `Horizontal Box` à deux `Vertical Box` :
   - gauche : `Button` `UnassignedButton` (texte « Non assignés »), puis `Vertical Box` `UnassignedList` ;
   - droite, une section par valeur de `EServantTask` hors `Unassigned` : `AttackDefenseButton` (« Attaque / défense ») + `AttackDefenseList`, puis `FieldHarvestButton` (« Récolte de champ ») + `FieldHarvestList`.
   Tous en *Is Variable* ; **chaque bouton en `Is Focusable` décoché** (sinon Tab part dans la navigation Slate et ne ferme plus le menu).
7. [HUMAIN] Fonction `FillList(Box: Vertical Box, Task: EServantTask)` : `Box > Clear Children` → `Get Servant Subsystem` → `Is Valid` → `Get Servants With Task(Task)` → `For Each Loop` → `Create Widget` (`WBP_ServantEntry`, Owning Player = `Get Owning Player`, Servant = élément, ServantNumber = `Get Servant Number(élément)`, OwnerMenu = `Self`) → `Box > Add Child`.
8. [HUMAIN] Fonction `ApplyHighlight(Selection: Servant)` : pour chacune des 3 listes, `Get All Children` → `For Each Loop` → `Cast To WBP_ServantEntry` → `SetHighlighted(Equal (Object)(Servant, Selection))`.
9. [HUMAIN] *Functions > Override* > `On Menu Refresh Requested` → `FillList` ×3 (`Unassigned`, `AttackDefense`, `FieldHarvest`) → `ApplyHighlight(Get Selected Servant)` (la sélection survit au retrait d'un autre serviteur). Override `On Selection Changed(NewSelection)` → `ApplyHighlight(NewSelection)`.
10. [HUMAIN] *On Clicked* des boutons de section → `Assign Selected Servant` : `UnassignedButton` → `Unassigned`, `AttackDefenseButton` → `AttackDefense`, `FieldHarvestButton` → `FieldHarvest`.
11. [HUMAIN] `BP_FirstPersonCharacter`, à la suite de l'`Event BeginPlay` existant (même chaîne et même Owning Player que `WBP_ControlsPanel`) : `Create Widget` (`WBP_ServantTaskMenu`) → *Promote to Variable* `ServantTaskMenu` → `Add to Viewport` → `Set Visibility` = `Collapsed` → nœud `Get ServantSubsystem` (rechercher « ServantSubsystem », sous-système de monde) → `Initialize Menu` (cible `ServantTaskMenu`).
12. [HUMAIN] `Event On Servant Menu Toggled(bIsOpen)` → `Is Valid (ServantTaskMenu)` → `Select` (vrai `Visible`, faux `Collapsed`) → `Set Visibility`. Pause, curseur et mode d'entrée sont déjà gérés en C++ : rien d'autre à ajouter.

**Le widget est créé une seule fois et affiché/masqué par visibilité, jamais `Remove from Parent` puis réajouté au viewport : son `NativeDestruct` coupe l'abonnement au registre et le menu ne se rafraîchirait plus.**
**Pas de `Delay` dans une fonction Blueprint** (`FillList`, `ApplyHighlight`, `SetHighlighted`) : passer par un Custom Event de l'Event Graph si un délai devient nécessaire.

## Test PIE (Timéo), `Lvl_Principal`, au moins 2 Necrospores
1. Poser 2 serviteurs (L) : ils errent comme avant. Tab : jeu en pause (serviteurs figés), curseur visible, « Non assignés » = Serviteur 1, Serviteur 2. Tab : reprise, curseur masqué, regard souris rétabli (critère 9).
2. Menu ouvert : ZQSD, souris, E, L, clic gauche, clic droit et Entrée sont sans effet. Cliquer un bouton puis Tab : le menu se ferme (Tab n'est pas capté par les boutons).
3. Clic Serviteur 1 (surbrillance) puis « Récolte de champ » : il change de colonne tout de suite, menu toujours ouvert, surbrillance retirée. Fermer, rouvrir : l'affectation est conservée (critère 10). Clic sur une section sans sélection : rien ne se passe.
4. Clic dans le vide (fond du menu) : le curseur reste visible, rien ne se déclenche, Tab ferme toujours.
5. Menu ouvert, sélectionner Serviteur 2, puis supprimer son acteur dans l'Outliner (monde PIE) : il disparaît de sa colonne, sans ligne fantôme ni surbrillance restante. Un serviteur posé ensuite s'appelle « Serviteur 3 ».
6. Courir avec Maj maintenue, Tab, relâcher Maj, Tab : on marche à la vitesse normale (sprint non bloqué) ; un nouvel appui sur Maj fait sprinter.
Mécaniques impactées (diff : `BeginPlay`/`EndPlay` de `Servant`, bindings du Character) : pose de serviteur (L, consomme une Necrospore), errance, panneau de contrôles (Entrée), sprint et endurance, saisir/relâcher/interagir (E).
