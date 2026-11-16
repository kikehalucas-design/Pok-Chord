# PokéChord : générateur d'accords (VST3) pour Ableton

Tu choisis un **style** (House, Deep House, Afro House, Disco / Funk, Jazz, Neo-Soul, Lo-Fi, R&B, Pop, Techno), une **humeur** (Dansant, Joyeux, Énergique, Romantique, Planant, Nostalgique, Mélancolique, Triste, Sombre, Mystérieux, Tendu), et le plugin invente une suite d'accords. Chaque clic sur la Poké Ball en donne une nouvelle.

## Comment récupérer les accords dans Ableton

Ableton ne permet pas à un plugin VST d'envoyer du MIDI vers une autre piste. Le plugin passe donc par un **clip MIDI** :

1. Mets PokéChord sur une piste (c'est maintenant un **effet audio** : pose-le sur une **piste audio**, dans Plug-ins > Audio Effects). Clique sur **la Poké Ball** : tu entends la suite d'accords tout de suite.
2. Quand une suite te plaît, **attrape la zone en pointillés "GLISSE LE MIDI DANS ABLETON"** et glisse-la sur une **piste MIDI** (ou une zone vide d'Ableton pour créer une piste). Le clip contient les notes, tu peux les modifier comme n'importe quel clip.
3. Mets ton propre synthé ou ton piano sur cette piste MIDI : les accords jouent avec ton son.
4. Si le glisser-déposer ne marche pas, utilise le bouton **ENREGISTRER .MID**, puis glisse le fichier depuis le Finder dans Ableton.

Quand tu es sur la piste MIDI avec ton propre son, tu peux couper "SON D'ÉCOUTE" ou retirer le plugin.

## Obtenir le plugin (GitHub)

1. Dans ton dépôt GitHub, ajoute le dossier `PokeChord` (le même principe que pour Minimal Transformer).
2. Ouvre `.github/workflows/main.yml`, remplace tout son contenu par celui du fichier `build.yml` de ce zip, puis **Commit changes**.
3. Onglet **Actions** : la compilation tourne (10 à 15 minutes), puis dans **Artifacts** télécharge `PokeChord-macOS-VST3` (ou Windows).

## Installer (Mac)

Décompresse l'archive téléchargée, puis dans le Terminal :

```
S="$(find ~/Downloads -maxdepth 4 -type d -exec test -f '{}/Contents/MacOS/PokeChord' \; -print | head -1)"; D=~/Library/Audio/Plug-Ins/VST3/PokeChord.vst3; [ -n "$S" ] && mkdir -p ~/Library/Audio/Plug-Ins/VST3 && rm -rf "$D" && cp -R "$S" "$D" && find "$D" -name .DS_Store -delete && chmod +x "$D/Contents/MacOS/PokeChord" && xattr -cr "$D" && codesign --force --deep -s - "$D" && echo "Installé, relance Ableton" || echo "Échec"
```

Puis dans Ableton : ferme et rouvre, **Live > Réglages > Plug-ins**, **Rescanner** (avec la touche Option pour un scan complet).

## Les réglages

- **Style / Humeur / Tonalité / Mode** (menus déroulants) : "Aléatoire" laisse le plugin choisir. Le mode majeur ou mineur dépend de l'humeur.
- **Durée d'un accord** : 1/2 mesure, 1 mesure ou 2 mesures. **Random** choisit une durée au hasard à chaque génération (même chose pour **Rythme** et **Disposition**). **Accords** : de 2 à 8.
- **Complexité** : accords simples (triades) vers accords riches (7e, 9e, 11e, 13e).
- **Audace** : ajoute des dominantes secondaires, des substitutions et des emprunts à l'autre mode.
- **Humaniser** : petites variations de vélocité (jusqu'à ±14) et léger décalage des notes d'un accord, comme une main qui joue.
- **Vélo random** : variation aléatoire des vélocités, jusqu'à ±45 au maximum, en plus de l'humanisation. Le volume du son d'écoute se règle avec le fader de la piste dans Ableton.
- **Rythme** : tenu, offbeat house, syncopé, stabs funk, arpèges, etc.
- **Preset d'écoute** : choisis le son utilisé pour écouter les accords (Piano house, Rhodes, Orgue house, Stab, Supersaw, Pad, Pluck, Cordes, Synth). Ce sont des approximations de sons classiques de la house, uniquement pour l'écoute dans le plugin : une fois le MIDI glissé, tu utilises ton propre instrument.
- **Disposition** : serrée, ouverte (drop 2) ou piano (main gauche + main droite). **Octave** : monte ou descend le registre. **Basse** : ajoute la fondamentale dans le grave.
- **◀** : revient à la suite précédente. **▶** : relance la lecture dans le plugin (elle suit aussi le transport d'Ableton).

La graine est sauvegardée avec ton projet : en rouvrant le set, tu retrouves exactement les mêmes accords.


Chaque humeur a son Pokémon et la couleur de son type : Dansant (Salamèche), Joyeux (Évoli), Énergique (Pikachu), Romantique (Rondoudou), Planant (Ronflex), Nostalgique (Bulbizarre), Mélancolique (Carapuce), Triste (Mewtwo), Sombre (Ectoplasma), Mystérieux (Mew), Tendu (Dracaufeu). 

## L'onglet ANALYSER

Clique sur **ANALYSER** en haut à droite : Noctali écoute l'audio qui passe sur la piste et affiche l'accord entendu, les notes détectées et la liste des accords au fil du morceau.

1. Mets PokéChord sur la piste audio à analyser (une piste d'accords, un sample, un morceau) et lance la lecture.
2. L'accord apparaît en grand, avec les notes entendues. Un accord est ajouté à la liste quand il reste stable un instant.
3. Attrape **GLISSE LES ACCORDS EN MIDI DANS ABLETON** et glisse-le sur une piste MIDI pour récupérer les accords détectés en clip MIDI.
4. **PAUSE / ÉCOUTER** arrête ou relance l'écoute, **EFFACER** vide la liste.

L'analyse marche mieux sur des accords tenus et clairs, avec peu de batterie. L'audio passe dans le plugin sans être modifié.

## D'où viennent les accords

- **Progressions** : plus de 650 progressions classées par style (10), par humeur (11) et par longueur (2 à 8 accords), construites à partir d'enchaînements très courants (I–V–vi–IV, vi–IV–I–V, ii–V–I, i–bVII–bVI, i–bVI–bIII–bVII, vamps dorien…). Elles ne sont pas tirées d'une base de données par style : pour la house, la deep house, la soul ou le lo-fi, ce sont des choix musicaux, pas des statistiques mesurées.
- **Générateur harmonique** : en plus de la bibliothèque, le plugin invente des progressions en suivant les enchaînements les plus courants (IV→V, ii→V, vi→IV, en mineur i→bVII→bVI…). L'humeur et le style modifient les probabilités, donc chaque clic peut donner une suite nouvelle, quelle que soit la longueur choisie (plus de 1 900 progressions différentes rien que pour 8 accords).
- **Humeur** : le mode (majeur ou mineur) est la partie la mieux établie par la recherche. Le tempo pèse autant que le mode sur l'émotion perçue, c'est à toi de le régler dans Ableton.
- **Accords** : les couleurs (7e, 9e, 11e, 13e, sus…) respectent la tonalité : un V devient une dominante, un sus4 n'est jamais placé là où il sortirait de la gamme.
- **Mode** : "Auto" suit l'humeur (majeur pour Joyeux, mineur pour Triste…). "Major" ou "Minor" force le mode : par exemple Triste en majeur, ou Joyeux en mineur.
- **Piano roll** : sous les menus Style et Humeur, il affiche toutes les notes générées (les basses en couleur), les accords et la tête de lecture quand le son d'écoute joue.

## Nouveau look (v3)
- Toute l'interface prend la couleur du type du Pokémon de l'humeur (faders, tête de lecture, onglet, accord qui joue).
- Police Poppins intégrée au plugin (rien à installer).
- Cartes avec ombre douce, Pokémon qui respire, Poké Ball des faders qui roule, notes du piano roll qui s'allument pendant l'écoute, pilule sur l'accord en cours.
