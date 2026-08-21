# MaterialTheme.cpp — token diff

**No colour changes.** `LightNeutrals`, `DarkNeutrals`, `LightStatus`,
`DarkStatus` and all four `SeedPalettes` entries are used verbatim. The
Expressive rethink changes shape, motion and type, not the palette — a password
manager's status colours are load-bearing and were already right.

`lib/kpxc.css` is the machine-checkable copy: every hex in it appears in
`MaterialTheme.cpp`.

## Shape — additions

```diff
 namespace Shape
 {
     constexpr int None = 0;
     constexpr int ExtraSmall = 6;
     constexpr int Small = 8;
     constexpr int Medium = 12;
     constexpr int Large = 14;
     constexpr int Row = 16;
     constexpr int Rail = 18;
     constexpr int ExtraLarge = 28;
     constexpr int Full = 999;
+
+    // M3 Expressive press states. A component morphs to its Pressed radius
+    // while held and springs back on release; see MaterialShapeMorph.
+    constexpr int RailPressed = 10;
+    constexpr int RowPressed = 8;
+    constexpr int ChipPressed = 16;
+    constexpr int ButtonPressed = 14;
+    constexpr int FabExpanded = 16;
 } // namespace Shape
```

## Duration — additions

```diff
 namespace Duration
 {
     constexpr int Short = 140;
     constexpr int Toggle = 160;
     constexpr int Medium = 180;
     constexpr int Long = 240;
     constexpr int Toast = 4200;
+
+    // Expressive motion. Spring is the shape morph and the FAB expand; Stagger
+    // is the per-item delay in a FAB menu or a list reveal.
+    constexpr int Spring = 200;
+    constexpr int Stagger = 40;
 } // namespace Duration
```

## Layout — additions

```diff
 namespace Layout
 {
     constexpr int RailWidth = 88;
     constexpr int RailItemWidth = 66;
     ...
     constexpr int IconButtonSize = 40;
+
+    // The icons-only rail at the Medium breakpoint, and the bottom bar that
+    // replaces it below 600. See MaterialBreakpoints.h.
+    constexpr int RailWidthCompactLabels = 72;
+    constexpr int BottomBarHeight = 76;
+    constexpr int BottomBarDestinations = 5;
+    constexpr int WindowChromeHeight = 44;
+    constexpr int CaptionButtonWidth = 46;
+    constexpr int NotificationDrawerWidth = 420;
+    constexpr int InspectorDrawerWidth = 440;
 } // namespace Layout
```

## Theme — new state

```diff
 class Theme : public QObject
 {
     ...
     Density density() const;
+
+    /** Interface font family, or an empty string to follow the platform. */
+    QString fontFamily() const;
+    void setFontFamily(const QString& family);
+
+    /** Type scale multiplier, 0.85 to 1.40. Applies to every TypeRole. */
+    qreal fontScale() const;
+    void setFontScale(qreal scale);
+
+    /** Base weight: 300, 400 or 500. Medium roles add 100 on top. */
+    int fontWeight() const;
+    void setFontWeight(int weight);
 ```

`font(TypeRole)` multiplies the role's point size by `fontScale()` and adds
`fontWeight() - 400` to the role's weight. Two consequences worth stating:

- **The CJK fallback must survive a family change.** `uiFamily()` returns a
  chain, not a name, and the chain always ends in a CJK face. A user who picks
  a Latin-only family must not lose Cantonese to tofu blocks.
- **Every layout must hold at 140%.** That is the scale where clipping shows up
  first, and it is the one to test with the longest localized string.

## New Config keys

```
GUI_Language                 enum en|yue|both        default "en"
GUI_FunnyLevelEn             int 1-5                 default 3
GUI_FunnyLevelYue            int 1-5                 default 3
GUI_NarratorEnabled          bool                    default false
GUI_FontFamily               QString                 default ""
GUI_FontScale                double 0.85-1.40        default 1.0
GUI_FontWeight               int                     default 400
GUI_ElementOverrides         QJsonObject             default {}
GUI_MaterialRailSublabels    bool                    default true
GUI_CustomWindowChrome       bool                    default true (Windows)
GUI_TabOrder                 QStringList             default {}
GUI_PinnedTabs               QStringList             default {}
GUI_TabOverflow              bool                    default true
GUI_ShowTabStrip             bool                    default true
GUI_SearchRegexDefault       bool                    default false
GUI_SearchFlags              QString                 default "gi"
GUI_RegexPresets             QStringList of JSON     default {}
GUI_RegexLastPattern         QString                 default ""
GUI_NotificationTimeout      int ms                  default 4200
GUI_NotificationHistoryLimit int                     default 200
GUI_NotificationAnnounce     bool                    default true
GUI_DimSumEnabled            bool                    default true
GUI_ChangelogLastSeen        QString                 default ""
GUI_ExternalEditorPath       QString                 default ""
GUI_ExternalEditorArgs       QString                 default "%1"
History_Enabled              bool                    default true
History_RetentionDays        int                     default 365
History_MaxRevisions         int                     default 200
History_IncludeSettings      bool                    default true
```
