#include "PluginEditor.h"
#include "Shapes.h"

using namespace juce;

namespace
{
const Colour kYellow  (0xffffdd2d);
const Colour kYellowD (0xff8f7c1a);
const Colour kBg      (0xff1b1b1b);
const Colour kCell    (0xff2a2a2a);
const Colour kGrey    (0xff8f8d90);
const Colour kDark    (0xff111111);

// zones (coordonnées de référence)
const Rectangle<float> kDisplay   (865.f, 132.f, 1523.f, 716.f);
const Rectangle<float> kKnobArea  (122.f, 294.f, 620.f, 620.f);
const Rectangle<float> kBandPill  (44.f, 1056.f, 200.f, 76.f);
const Rectangle<float> kListen    (254.f, 1056.f, 90.f, 76.f);
const Rectangle<float> kSlider    (44.f, 1160.f, 764.f, 44.f);
const Rectangle<float> kLoop      (2180.f, 40.f, 80.f, 64.f);
const Rectangle<float> kMenu      (2310.f, 35.f, 90.f, 70.f);

Rectangle<float> navRect (int i)
{
    static const float xs[4] = { 865.f, 1057.f, 2017.f, 2209.f };
    return { xs[i], 872.f, 178.f, 80.f };
}
Rectangle<float> shapeRect (int i)
{
    return { 865.f + (float) (i % 8) * 192.f, 984.f + (float) (i / 8) * 116.f, 180.f, 104.f };
}
Font bold (float size, const String& face = {})
{
    return face.isEmpty() ? Font (FontOptions (size, Font::bold)) : Font (FontOptions (face, size, Font::bold));
}
}

KickEditor::KickEditor (KickProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      mixA    (*p.apvts.getParameter ("mix"),    [this] (float v) { mix = v;               repaint(); }),
      divA    (*p.apvts.getParameter ("div"),    [this] (float v) { div = (int) std::round (v);   repaint(); }),
      modeA   (*p.apvts.getParameter ("mode"),   [this] (float v) { mode = (int) std::round (v);  repaint(); }),
      loopA   (*p.apvts.getParameter ("loop"),   [this] (float v) { loop = v > 0.5f;       repaint(); }),
      shapeA  (*p.apvts.getParameter ("shape"),  [this] (float v) { shape = (int) std::round (v); repaint(); }),
      lenA    (*p.apvts.getParameter ("len"),    [this] (float v) { len = v;               repaint(); }),
      bandA   (*p.apvts.getParameter ("band"),   [this] (float v) { band = v > 0.5f;       repaint(); }),
      listenA (*p.apvts.getParameter ("listen"), [this] (float v) { listen = v > 0.5f;     repaint(); }),
      freqA   (*p.apvts.getParameter ("freq"),   [this] (float v) { freq = v;              repaint(); })
{
    for (auto* a : { &mixA, &divA, &modeA, &loopA, &shapeA, &lenA, &bandA, &listenA, &freqA })
        a->sendInitialUpdate();

    setResizeLimits (604, 308, 2416, 1232);
    getConstrainer()->setFixedAspectRatio (2416.0 / 1232.0);
    setSize (1208, 616);
    setResizable (true, true);
    startTimerHz (30);
}

Rectangle<float> KickEditor::divRect (int i) const
{
    static const float cx[4] = { 938.f, 1094.f, 1251.f, 1406.f };
    return { cx[i] - 74.f, 32.f, 148.f, 76.f };
}
Rectangle<float> KickEditor::modeRect (int i) const
{
    static const float cx[3] = { 1654.f, 1850.f, 2060.f };
    static const float w[3]  = { 190.f, 190.f, 224.f };
    return { cx[i] - w[i] / 2.f, 32.f, w[i], 76.f };
}
float KickEditor::sliderNorm() const
{
    return proc.apvts.getParameter ("freq")->convertTo0to1 (freq);
}

// ------------------------------------------------------------------ dessin
void KickEditor::drawPill (Graphics& g, Rectangle<float> r, const String& text, bool on, float fs)
{
    if (on)
    {
        g.setColour (kYellow);
        g.fillRoundedRectangle (r, r.getHeight() / 2.f);
    }
    g.setColour (on ? kDark : kGrey.withAlpha (0.95f));
    g.setFont (bold (fs, "Verdana"));
    g.drawText (text, r, Justification::centred);
}

void KickEditor::drawShapeIcon (Graphics& g, Rectangle<float> r, int idx, Colour c, float width)
{
    auto a = r.reduced (26.f, 24.f);
    Path p;
    const int steps = 80;
    float lastY = 0.f;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = a.getX() + t * (a.getWidth() - 10.f);
        const float y = a.getBottom() - KS::shapeValue (idx, t) * a.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        lastY = y;
    }
    const float xe = a.getX() + a.getWidth() - 10.f;
    p.lineTo (xe, jmin (a.getBottom(), lastY + a.getHeight() * 0.5f));
    g.setColour (c);
    g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
}

void KickEditor::drawTopBar (Graphics& g)
{
    static const char* divNames[4] = { "1/8", "1/4", "1/2", "1/1" };
    for (int i = 0; i < 4; ++i)
        drawPill (g, divRect (i), divNames[i], div == i);

    static const char* modeNames[3] = { "SYNC", "MIDI", "AUDIO" };
    for (int i = 0; i < 3; ++i)
        drawPill (g, modeRect (i), modeNames[i], mode == i);

    // icône de boucle
    const auto c = kLoop.getCentre();
    Path loopP;
    loopP.addCentredArc (c.x, c.y, 20.f, 20.f, 0.f, 0.5f, 3.0f, true);
    loopP.addCentredArc (c.x, c.y, 20.f, 20.f, 0.f, 3.6f, 6.1f, true);
    g.setColour (loop ? kYellow : kGrey);
    g.strokePath (loopP, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
    Path a1; a1.addTriangle (c.x + 14.f, c.y - 24.f, c.x + 14.f, c.y - 6.f, c.x + 30.f, c.y - 14.f);
    Path a2; a2.addTriangle (c.x - 14.f, c.y + 24.f, c.x - 14.f, c.y + 6.f, c.x - 30.f, c.y + 14.f);
    g.fillPath (a1); g.fillPath (a2);

    // menu
    g.setColour (Colour (0xffd8d8d8));
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (2325.f, 50.f + (float) i * 21.f, 60.f, 4.f, 2.f);
}

void KickEditor::drawKnob (Graphics& g)
{
    const Point<float> c (432.f, 604.f);

    g.setColour (Colour (0xff141414));                       // ombre
    g.fillEllipse (c.x - 306.f, c.y - 296.f, 612.f, 612.f);
    g.setColour (Colour (0xff303030));
    g.fillEllipse (c.x - 284.f, c.y - 284.f, 568.f, 568.f);
    g.setColour (Colour (0xff1e1e1e));
    g.fillEllipse (c.x - 240.f, c.y - 240.f, 480.f, 480.f);
    g.setColour (Colour (0xff353535));
    g.fillEllipse (c.x - 196.f, c.y - 196.f, 392.f, 392.f);
    g.setColour (Colour (0xff1b1b1b));
    g.fillEllipse (c.x - 76.f, c.y - 76.f, 152.f, 152.f);

    const float start = -2.356f, sweep = 4.712f;
    const float ang = start + mix * sweep;

    Path arc;
    arc.addCentredArc (c.x, c.y, 296.f, 296.f, 0.f, start, ang, true);
    g.setColour (kYellow);
    g.strokePath (arc, PathStrokeType (26.f, PathStrokeType::curved, PathStrokeType::rounded));

    // repère sur le bouton
    const float px = c.x + std::sin (ang) * 176.f, py = c.y - std::cos (ang) * 176.f;
    g.setColour (Colour (0xff4a4a4a));
    g.fillEllipse (px - 17.f, py - 17.f, 34.f, 34.f);
    Path wedge;
    wedge.startNewSubPath (c.x + std::sin (ang - 0.14f) * 90.f, c.y - std::cos (ang - 0.14f) * 90.f);
    wedge.lineTo (c.x + std::sin (ang - 0.20f) * 232.f, c.y - std::cos (ang - 0.20f) * 232.f);
    wedge.lineTo (c.x + std::sin (ang + 0.20f) * 232.f, c.y - std::cos (ang + 0.20f) * 232.f);
    wedge.lineTo (c.x + std::sin (ang + 0.14f) * 90.f, c.y - std::cos (ang + 0.14f) * 90.f);
    wedge.closeSubPath();
    g.setColour (Colour (0xff3b3b3b).withAlpha (0.6f));
    g.fillPath (wedge);

    g.setColour (kGrey);
    g.setFont (bold (54.f, "Verdana"));
    g.drawText ("MIX " + String (roundToInt (mix * 100.f)) + "%", Rectangle<float> (150.f, 930.f, 580.f, 70.f), Justification::centred);
}

void KickEditor::drawBottomLeft (Graphics& g)
{
    drawPill (g, kBandPill, "BAND", band, 46.f);

    // casque
    const auto c = kListen.getCentre();
    g.setColour (listen ? kYellow : kGrey);
    Path head; head.addCentredArc (c.x, c.y + 4.f, 20.f, 20.f, 0.f, -1.6f, 1.6f, true);
    g.strokePath (head, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.fillRoundedRectangle (c.x - 28.f, c.y + 2.f, 14.f, 24.f, 5.f);
    g.fillRoundedRectangle (c.x + 14.f, c.y + 2.f, 14.f, 24.f, 5.f);

    // slider de fréquence
    g.setColour (kCell);
    g.fillRoundedRectangle (kSlider, 22.f);
    const float tx = kSlider.getX() + 32.f + sliderNorm() * (kSlider.getWidth() - 64.f);
    g.setColour (kYellow);
    g.fillRoundedRectangle (kSlider.withRight (tx + 4.f), 22.f);
    g.setColour (Colour (0xff595959));
    g.fillEllipse (tx - 32.f, kSlider.getCentreY() - 32.f, 64.f, 64.f);
    g.setColour (Colour (0xff2a2a2a));
    g.fillEllipse (tx - 12.f, kSlider.getCentreY() - 12.f, 24.f, 24.f);
}

void KickEditor::drawDisplay (Graphics& g)
{
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (kDisplay, 14.f);
    g.reduceClipRegion (clip);

    g.setColour (Colour (0xff0e0e0e));
    g.fillRect (kDisplay);

    const float x0 = kDisplay.getX(), w = kDisplay.getWidth();
    const int cols = KickProcessor::kCols;
    const float colW = w / (float) cols;
    const float cy = 490.f, amp = 330.f;

    // forme d'onde : entrée en gris, sortie en jaune sombre
    for (int i = 0; i < cols; ++i)
    {
        const float x = x0 + ((float) i + 0.5f) * colW;
        const float a = proc.inMax[i].load(), b = proc.inMin[i].load();
        g.setColour (Colour (0xff8a8890).withAlpha (0.85f));
        g.fillRect (x - colW * 0.5f, cy - jlimit (-1.f, 1.f, a) * amp, colW + 0.5f, jmax (2.f, (a - b) * amp));
        const float oa = proc.outMax[i].load(), ob = proc.outMin[i].load();
        g.setColour (kYellowD.withAlpha (0.9f));
        g.fillRect (x - 1.5f, cy - jlimit (-1.f, 1.f, oa) * amp, 3.f, jmax (2.f, (oa - ob) * amp));
    }

    // ligne du zéro
    g.setColour (kYellowD.withAlpha (0.8f));
    g.fillRect (x0, cy - 1.f, w, 3.f);

    // courbe
    const float lx = x0 + len * w;
    auto yOf = [] (float v) { return 810.f - v * 645.f; };
    Path curve;
    const int steps = 300;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = x0 + 3.f + t * (lx - x0 - 3.f);
        const float y = yOf (KS::shapeValue (shape, t));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    const float endV = KS::shapeValue (shape, 1.f);
    Path tail;
    tail.startNewSubPath (lx, yOf (endV));
    tail.lineTo (kDisplay.getRight() - 12.f, yOf (endV));
    tail.lineTo (kDisplay.getRight() - 12.f, 655.f);
    g.setColour (kYellowD);
    g.strokePath (tail, PathStrokeType (7.f));
    g.setColour (kYellow);
    g.strokePath (curve, PathStrokeType (8.f, PathStrokeType::curved, PathStrokeType::rounded));

    // position de lecture
    const float ph = proc.uiPhase.load();
    const float tt = jmin (1.f, ph / len);
    const float dotX = x0 + 3.f + jmin (ph, len) / len * (lx - x0 - 3.f);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.fillEllipse (dotX - 10.f, yOf (KS::shapeValue (shape, tt)) - 10.f, 20.f, 20.f);

    // zone après la longueur + poignée
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (Rectangle<float> (lx, kDisplay.getY(), kDisplay.getRight() - lx, kDisplay.getHeight()));
    g.setColour (Colour (0xffcfcfcf));
    g.fillRect (lx - 1.5f, kDisplay.getY(), 3.f, kDisplay.getHeight());
    g.setColour (Colour (0xff3b3b3b));
    g.fillRoundedRectangle (lx - 80.f, 185.f, 78.f, 64.f, 8.f);
    g.setColour (Colour (0xffdadada));
    for (int i = 0; i < 3; ++i)
        g.fillRect (lx - 62.f + (float) i * 17.f, 200.f, 5.f, 34.f);
}

void KickEditor::drawNavAndShapes (Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        auto r = navRect (i);
        g.setColour (kCell);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (kGrey);
        const auto c = r.getCentre();
        const bool left = i < 2;
        const float d = left ? -1.f : 1.f;
        if (i == 0 || i == 3)
        {
            Path t; t.addTriangle (c.x - d * 16.f, c.y - 24.f, c.x - d * 16.f, c.y + 24.f, c.x + d * 18.f, c.y);
            g.fillPath (t);
        }
        else
        {
            Path ch;
            ch.startNewSubPath (c.x - d * 14.f, c.y - 24.f); ch.lineTo (c.x + d * 16.f, c.y); ch.lineTo (c.x - d * 14.f, c.y + 24.f);
            g.strokePath (ch, PathStrokeType (7.f, PathStrokeType::mitered, PathStrokeType::butt));
        }
    }

    for (int i = 0; i < KS::numShapes; ++i)
    {
        auto r = shapeRect (i);
        const bool on = (i == shape);
        g.setColour (on ? kYellow : kCell);
        g.fillRoundedRectangle (r, 8.f);
        drawShapeIcon (g, r, i, on ? kDark : Colour (0xff8a8a8a), on ? 8.f : 6.f);
    }
}

void KickEditor::paint (Graphics& g)
{
    g.fillAll (kBg);
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::scale (refScale()));

    // titre (nom modifiable ici)
    g.setColour (Colour (0xff3a3a3a));
    g.setFont (Font (FontOptions ("Arial", 74.f, Font::bold | Font::italic)));
    g.drawText ("SIDECHAIN", Rectangle<float> (200.f, 56.f, 520.f, 84.f), Justification::centredLeft);
    g.setColour (kYellow);
    g.setFont (bold (86.f, "Arial Black"));
    g.drawText ("KICKSTART", Rectangle<float> (112.f, 124.f, 700.f, 96.f), Justification::centredLeft);

    drawTopBar (g);
    drawDisplay (g);
    drawNavAndShapes (g);
    drawKnob (g);
    drawBottomLeft (g);
}

// ------------------------------------------------------------------ souris
void KickEditor::mouseDown (const MouseEvent& e)
{
    const auto q = toRef (e);
    drag = Drag::none;

    for (int i = 0; i < 4; ++i)
        if (divRect (i).expanded (20.f, 0.f).contains (q)) { divA.setValueAsCompleteGesture ((float) i); return; }
    for (int i = 0; i < 3; ++i)
        if (modeRect (i).contains (q)) { modeA.setValueAsCompleteGesture ((float) i); return; }

    if (kLoop.contains (q))   { loopA.setValueAsCompleteGesture (loop ? 0.f : 1.f); return; }
    if (kBandPill.contains (q)) { bandA.setValueAsCompleteGesture (band ? 0.f : 1.f); return; }
    if (kListen.contains (q))   { listenA.setValueAsCompleteGesture (listen ? 0.f : 1.f); return; }

    if (kMenu.contains (q))
    {
        PopupMenu m;
        m.addItem (1, "Reset");
        m.addItem (2, "About");
        m.showMenuAsync (PopupMenu::Options(), [this] (int r)
        {
            if (r == 1)
            {
                for (auto* prm : proc.getParameters())
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->getDefaultValue());
                    prm->endChangeGesture();
                }
            }
            else if (r == 2)
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Kickstart",
                    "Volume shaper sidechain (usage perso).\n"
                    "SYNC : suit le tempo de l'hote | MIDI : declenche sur les notes | AUDIO : declenche sur le son (entree ou sidechain).");
        });
        return;
    }

    for (int i = 0; i < 4; ++i)
        if (navRect (i).contains (q))
        {
            int n = shape;
            if (i == 0) n = 0;
            else if (i == 1) n = shape - 1;
            else if (i == 2) n = shape + 1;
            else n = KS::numShapes - 1;
            shapeA.setValueAsCompleteGesture ((float) jlimit (0, KS::numShapes - 1, n));
            return;
        }
    for (int i = 0; i < KS::numShapes; ++i)
        if (shapeRect (i).contains (q)) { shapeA.setValueAsCompleteGesture ((float) i); return; }

    if (kKnobArea.getCentre().getDistanceFrom (q) < 310.f)
    {
        drag = Drag::knob; dragStartValue = mix; mixA.beginGesture(); return;
    }
    if (kSlider.expanded (0.f, 30.f).contains (q))
    {
        drag = Drag::freqSlider; freqA.beginGesture(); mouseDrag (e); return;
    }
    const float lx = kDisplay.getX() + len * kDisplay.getWidth();
    if (kDisplay.contains (q) && std::abs (q.x - lx) < 90.f)
    {
        drag = Drag::length; lenA.beginGesture(); mouseDrag (e); return;
    }
}

void KickEditor::mouseDrag (const MouseEvent& e)
{
    const auto q = toRef (e);
    if (drag == Drag::knob)
    {
        const float dy = (float) e.getDistanceFromDragStartY() / refScale();
        mixA.setValueAsPartOfGesture (jlimit (0.f, 1.f, dragStartValue - dy / 450.f));
    }
    else if (drag == Drag::length)
    {
        const float n = (q.x - kDisplay.getX()) / kDisplay.getWidth();
        lenA.setValueAsPartOfGesture (jlimit (0.05f, 1.f, n));
    }
    else if (drag == Drag::freqSlider)
    {
        const float n = jlimit (0.f, 1.f, (q.x - kSlider.getX() - 32.f) / (kSlider.getWidth() - 64.f));
        freqA.setValueAsPartOfGesture (proc.apvts.getParameter ("freq")->convertFrom0to1 (n));
    }
}

void KickEditor::mouseUp (const MouseEvent&)
{
    if (drag == Drag::knob) mixA.endGesture();
    else if (drag == Drag::length) lenA.endGesture();
    else if (drag == Drag::freqSlider) freqA.endGesture();
    drag = Drag::none;
}

void KickEditor::mouseDoubleClick (const MouseEvent& e)
{
    const auto q = toRef (e);
    if (kKnobArea.getCentre().getDistanceFrom (q) < 310.f)
        mixA.setValueAsCompleteGesture (0.8f);
}

void KickEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (kKnobArea.getCentre().getDistanceFrom (toRef (e)) < 310.f)
        mixA.setValueAsCompleteGesture (jlimit (0.f, 1.f, mix + w.deltaY * 0.1f));
}
