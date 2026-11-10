#include "PluginEditor.h"
using namespace juce;
using namespace fireui;

// ------------------------------------------------------------------ look and feel
FireLNF::FireLNF() {
    setColour(ComboBox::backgroundColourId, kCard2);
    setColour(ComboBox::textColourId, kText);
    setColour(ComboBox::outlineColourId, kLine);
    setColour(ComboBox::arrowColourId, kGold);
    setColour(PopupMenu::backgroundColourId, kCard);
    setColour(PopupMenu::textColourId, kText);
    setColour(PopupMenu::highlightedBackgroundColourId, kFire);
    setColour(PopupMenu::highlightedTextColourId, kBg1);
    setColour(Slider::thumbColourId, kGold);
    setColour(Slider::trackColourId, kFire);
    setColour(Slider::backgroundColourId, kLine);
    setColour(Slider::textBoxTextColourId, kText);
    setColour(Slider::textBoxOutlineColourId, kLine);
}

void FireLNF::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool hl, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    if (on) g.setGradientFill(ColourGradient(kFire, r.getX(), r.getY(), kGold, r.getRight(), r.getBottom(), false));
    else    g.setColour(down ? kLine : (hl ? kCard2.brighter(0.15f) : kCard2));
    g.fillRoundedRectangle(r, 10.f);
    if (!on) { g.setColour(kLine); g.drawRoundedRectangle(r, 10.f, 1.f); }
    if (!b.isEnabled()) { g.setColour(Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(r, 10.f); }
}

void FireLNF::drawButtonText(Graphics& g, TextButton& b, bool, bool) {
    g.setColour(b.getToggleState() ? kBg1 : kText);
    g.setFont(Font(12.0f, Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4, 2), Justification::centred, 2);
}

// ------------------------------------------------------------------ drag pad
void DragPad::paint(Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(hover ? kCard2.brighter(0.2f) : kCard2);
    g.fillRoundedRectangle(r, 10.f);
    g.setColour(kFire);
    g.drawRoundedRectangle(r, 10.f, 1.5f);
    g.setColour(kText);
    g.setFont(Font(12.5f, Font::bold));
    g.drawText(label, getLocalBounds(), Justification::centred);
}

void DragPad::mouseDrag(const MouseEvent& e) {
    if (dragged || e.getDistanceFromDragStart() < 6) return;
    dragged = true;
    if (makeFile) {
        auto f = makeFile();
        if (f.existsAsFile())
            DragAndDropContainer::performExternalDragDropOfFiles(StringArray(f.getFullPathName()), false, this);
    }
}

void DragPad::mouseUp(const MouseEvent& e) {
    if (!dragged && e.getDistanceFromDragStart() < 6 && makeFile) {
        auto f = makeFile();
        auto dir = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("FireProgress");
        dir.createDirectory();
        auto out = dir.getChildFile(f.getFileName());
        if (f.copyFileTo(out) && onMessage) onMessage("Saved: " + out.getFullPathName());
    }
    dragged = false;
}

// ------------------------------------------------------------------ editor
FireProgressEditor::FireProgressEditor(FireProgressProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    setLookAndFeel(&lnf);
    setSize(1100, 720);
    setWantsKeyboardFocus(true);

    int id = 1;
    for (auto n : fire::NOTE) rootBox.addItem(n, id++);
    modeBox.addItem("Major", 1); modeBox.addItem("Minor", 2);
    barsBox.addItem("4 bars", 1); barsBox.addItem("8 bars", 2);
    for (int i = 1; i <= 8; ++i) countBox.addItem(String(i) + (i == 1 ? " chord" : " chords"), i);
    for (auto* c : { &rootBox, &modeBox, &barsBox, &countBox }) addAndMakeVisible(c);

    // step 1: world tradition, Indian raga. step 2: styles. (filled by hand, changes trigger side effects)
    tradBox.addItem("Choose a tradition...", 1);
    for (size_t i = 0; i < fire::trads().size(); ++i) tradBox.addItem(String(fire::trads()[i].name), (int) i + 2);
    ragaBox.addItem("Off", 1);
    for (size_t i = 0; i < fire::ragas().size(); ++i) ragaBox.addItem(String(fire::ragas()[i].name), (int) i + 2);
    genreBox.addItem("None", 1); electroBox.addItem("None", 1); otherBox.addItem("None", 1);
    {
        int gi = 2, ei = 2, oi = 2;
        for (auto& st : fire::styles()) {
            if (st.cat == 0) genreBox.addItem(String(st.name), gi++);
            else if (st.cat == 1) electroBox.addItem(String(st.name), ei++);
            else otherBox.addItem(String(st.name), oi++);
        }
    }
    for (auto* c : { &tradBox, &ragaBox, &genreBox, &electroBox, &otherBox }) addAndMakeVisible(c);
    tradBox.setSelectedItemIndex(geti("trad"), dontSendNotification);
    ragaBox.setSelectedItemIndex(geti("raga"), dontSendNotification);
    genreBox.setSelectedItemIndex(geti("sgen"), dontSendNotification);
    electroBox.setSelectedItemIndex(geti("selec"), dontSendNotification);
    otherBox.setSelectedItemIndex(geti("sother"), dontSendNotification);

    tradBox.onChange = [this] {
        setParam("trad", (float) tradBox.getSelectedItemIndex());
        applyStyleDefaults(); refreshSeed();
    };
    ragaBox.onChange = [this] {
        const int v = ragaBox.getSelectedItemIndex();
        setParam("raga", (float) v);
        if (v > 0) newGenerated();                       // raga mode: chords come from the raga only
        else { setParam("source", 0.f); setParam("prog", 0.f); }
        applyStyleDefaults(); refreshSeed();
    };
    auto styleChanged = [this] {
        setParam("sgen", (float) genreBox.getSelectedItemIndex());
        setParam("selec", (float) electroBox.getSelectedItemIndex());
        setParam("sother", (float) otherBox.getSelectedItemIndex());
        applyStyleDefaults(); refreshSeed();
    };
    genreBox.onChange = styleChanged; electroBox.onChange = styleChanged; otherBox.onChange = styleChanged;

    for (int i = 0; i < 7; ++i) {
        vibeBtn[i].setButtonText(fire::vibes()[(size_t) i].name);
        vibeBtn[i].onClick = [this, i] {
            setParam("vibe", (float) i); setParam("source", 0.f); setParam("prog", 0.f); refreshSeed();
        };
        addAndMakeVisible(vibeBtn[i]);
    }
    for (int i = 0; i < 3; ++i) {
        popBtn[i].onClick = [this, i] { selectProgression(0, i, geti("gen")); };
        genBtn[i].onClick = [this, i] { if (i < (int) genHist.size()) selectProgression(1, 0, genHist[(size_t) i]); };
        addAndMakeVisible(popBtn[i]); addChildComponent(genBtn[i]);
    }
    generateBtn.setToggleState(true, dontSendNotification);
    generateBtn.onClick = [this] { generate(); };
    newMelody.setToggleState(true, dontSendNotification);
    newMelody.onClick = [this] { refreshSeed(); };

    // sounds: Piano, Synth, Pad (for chords and for melody)
    const char* tones[3] = { "Piano", "Synth", "Pad" };
    for (int i = 0; i < 3; ++i) {
        cTone[i].setButtonText(tones[i]); mTone[i].setButtonText(tones[i]);
        cTone[i].onClick = [this, i] { setParam("csound", (float) i); };
        mTone[i].onClick = [this, i] { setParam("msound", (float) i); };
        addAndMakeVisible(cTone[i]); addAndMakeVisible(mTone[i]);
    }

    // history
    for (int i = 0; i < 4; ++i) {
        histBtn[i].onClick = [this, i] { jumpTo(histIdx[i]); };
        addChildComponent(histBtn[i]);
    }
    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };
    listBtn.onClick = [this] { showHistoryList(); };

    for (auto* b : { &generateBtn, &chordsBtn, &melodyBtn, &newMelody, &playBtn, &undoBtn, &redoBtn, &listBtn }) addAndMakeVisible(b);

    padChords.label = "Drag chords MIDI";
    padMelody.label = "Drag melody MIDI";
    padChords.makeFile = [this] { return proc.exportMidi(true); };
    padMelody.makeFile = [this] { return proc.exportMidi(false); };
    padChords.onMessage = padMelody.onMessage = [this](const String& m) { statusMsg = m; repaint(); };
    padChords.setMouseCursor(MouseCursor::DraggingHandCursor);
    padMelody.setMouseCursor(MouseCursor::DraggingHandCursor);
    addAndMakeVisible(padChords); addAndMakeVisible(padMelody);

    bpm.setSliderStyle(Slider::LinearHorizontal);
    bpm.setTextBoxStyle(Slider::TextBoxRight, false, 70, 22);
    bpm.setTextValueSuffix(" BPM");
    addAndMakeVisible(bpm);
    humanSlider.setSliderStyle(Slider::LinearHorizontal);
    humanSlider.setTextBoxStyle(Slider::TextBoxRight, false, 60, 22);
    humanSlider.setTextValueSuffix(" %");
    addAndMakeVisible(humanSlider);

    rootA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "root", rootBox);
    modeA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "mode", modeBox);
    barsA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "bars", barsBox);
    countA  = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "count", countBox);
    chordsA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "chords", chordsBtn);
    melodyA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "melody", melodyBtn);
    playA   = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "run", playBtn);
    bpmA    = std::make_unique<APVTS::SliderAttachment>(proc.apvts, "bpm", bpm);
    humanA  = std::make_unique<APVTS::SliderAttachment>(proc.apvts, "human", humanSlider);

    if (geti("source") == 1) genHist.push_back(geti("gen"));
    proc.recordSnapshot(snapshot());
    startTimerHz(30);
}

FireProgressEditor::~FireProgressEditor() { stopTimer(); setLookAndFeel(nullptr); }

void FireProgressEditor::setParam(const char* id, float v) {
    if (auto* prm = proc.apvts.getParameter(id)) {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(v));
        prm->endChangeGesture();
    }
}

void FireProgressEditor::syncCombo(ComboBox& box, const char* id) {
    const int v = geti(id);
    if (box.getSelectedItemIndex() != v) box.setSelectedItemIndex(v, dontSendNotification);
}

fire::Setup FireProgressEditor::setup() const {
    return fire::setupFromParams(geti("root"), geti("mode"), geti("vibe"), geti("prog"), geti("source"), geti("gen"),
                                 geti("bars"), geti("count"), geti("trad"), geti("raga"), geti("sgen"), geti("selec"), geti("sother"));
}
fire::Setup FireProgressEditor::setupFromSnap(const Snap& s) {
    return fire::setupFromParams(s[0], s[1], s[2], s[4], s[3], s[5], s[8], s[7], s[9], s[10], s[11], s[12], s[13]);
}

// ---------------------------------------------------------------- progressions + melody
// Every time the chords change (or you press New melody) we look for a melody that was never used before.
void FireProgressEditor::refreshSeed() {
    const auto s = setup();
    setParam("seed", (float) proc.freshSeed(s, fire::buildSegs(s)));
}

void FireProgressEditor::selectProgression(int source, int prog, int gen) {
    setParam("source", (float) source); setParam("prog", (float) prog); setParam("gen", (float) gen);
    refreshSeed();
}

// New generated progression from the main chord (or from the raga). Skips anything already shown.
void FireProgressEditor::newGenerated() {
    auto t = setup();
    int sd = 0;
    for (int tries = 0; tries < 40; ++tries) {
        sd = Random::getSystemRandom().nextInt(10000);
        t.gen = sd;
        const auto tok = fire::generatedTokens(t);
        bool dup = false;
        for (int h : genHist) { auto u = t; u.gen = h; if (fire::generatedTokens(u) == tok) dup = true; }
        for (int i = 0; i < (int) fire::popularList(t).size(); ++i) if (fire::popularTokens(t, i) == tok) dup = true;
        if (!dup) break;
    }
    genHist.insert(genHist.begin(), sd);
    if (genHist.size() > 3) genHist.pop_back();
    setParam("source", 1.f); setParam("gen", (float) sd);
}

void FireProgressEditor::generate() { newGenerated(); refreshSeed(); }

// Style / raga suggest a tempo (used by the standalone clock) and sounds. You can still change them afterwards.
void FireProgressEditor::applyStyleDefaults() {
    const auto mx = fire::mixOf(setup());
    setParam("bpm", (float) jlimit(40, 200, mx.bpm));
    if (mx.valid) { setParam("csound", (float) mx.cs); setParam("msound", (float) mx.ms); }
}

// ---------------------------------------------------------------- history + undo
FireProgressEditor::Snap FireProgressEditor::snapshot() const {
    return {{ geti("root"), geti("mode"), geti("vibe"), geti("source"), geti("prog"), geti("gen"), geti("seed"),
              geti("count"), geti("bars"), geti("trad"), geti("raga"), geti("sgen"), geti("selec"), geti("sother") }};
}

void FireProgressEditor::applySnapshot(const Snap& s) {
    static const char* ids[14] = { "root", "mode", "vibe", "source", "prog", "gen", "seed", "count", "bars",
                                   "trad", "raga", "sgen", "selec", "sother" };
    for (int i = 0; i < 14; ++i) setParam(ids[i], (float) s[(size_t) i]);
    if (s[3] == 1 && std::find(genHist.begin(), genHist.end(), s[5]) == genHist.end()) {
        genHist.insert(genHist.begin(), s[5]);
        if (genHist.size() > 3) genHist.pop_back();
    }
}

void FireProgressEditor::undo() { Snap s; if (proc.historyStep(-1, s)) applySnapshot(s); }
void FireProgressEditor::redo() { Snap s; if (proc.historyStep(+1, s)) applySnapshot(s); }
void FireProgressEditor::jumpTo(int index) { Snap s; if (proc.historyJump(index, s)) applySnapshot(s); }

bool FireProgressEditor::keyPressed(const KeyPress& k) {
    const auto m = k.getModifiers();
    if (m.isCommandDown()) {
        const int c = k.getKeyCode();
        if (c == 'Z' || c == 'z') { if (m.isShiftDown()) redo(); else undo(); return true; }
        if (c == 'Y' || c == 'y') { redo(); return true; }
    }
    return false;
}

String FireProgressEditor::labelOf(const fire::Setup& s) const {
    String out;
    if (fire::isRaga(s)) out = "Raga " + String(fire::ragas()[(size_t) s.raga].name);
    else {
        out = String(fire::tradOf(s).name).replace(" Music", "");
        if (fire::tradEff(s) == 2 && !fire::hasStyle(s)) out += " - " + String(fire::vibes()[(size_t) s.vibe].name);
        for (int k : { s.sg, s.se, s.so }) if (k >= 0) out += " + " + String(fire::styles()[(size_t) k].name);
    }
    return out + " - " + String(s.bars) + " bars";
}

String FireProgressEditor::describe(const Snap& sn) const {
    const auto s = setupFromSnap(sn);
    std::string names; int n = 0;
    for (auto& c : fire::resolveChords(s)) {
        if (n++ == 5) { names += " .."; break; }
        names += (names.empty() ? "" : " ") + fire::chordName(s.root, c);
    }
    return String(names) + "\n" + labelOf(s);
}

String FireProgressEditor::describeLine(const Snap& sn) const {
    const auto s = setupFromSnap(sn);
    const String key = String(fire::NOTE[s.root]) + (fire::isRaga(s) ? " (Sa)" : (s.minor ? " minor" : " major"));
    return key + "  |  " + describe(sn).replace("\n", "  |  ");
}

// chip text: Roman numerals + chord names (functional), or chord names + degrees (modal / raga)
String FireProgressEditor::chipText(const fire::Setup& s, const std::string& tok) const {
    const String names(fire::progNames(s, tok));
    if (fire::isModal(s)) return names + "\ndegrees " + String(tok);
    return String(tok) + "\n" + names;
}

void FireProgressEditor::showHistoryList() {
    PopupMenu menu;
    const int n = proc.historySize(), pos = proc.historyPos();
    if (n == 0) menu.addItem(1000, "No history yet", false, false);
    for (int i = n - 1; i >= 0 && i >= n - 40; --i)
        menu.addItem(i + 1, describeLine(proc.historyAt(i)), true, i == pos);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&listBtn),
                       [this](int r) { if (r > 0 && r < 1000) jumpTo(r - 1); });
}

// ---------------------------------------------------------------- timer
void FireProgressEditor::timerCallback() {
    const auto s = setup();
    const bool ragaOn = fire::isRaga(s), step1 = s.trad > 0 || ragaOn, modal = fire::isModal(s);

    proc.recordSnapshot(snapshot());

    syncCombo(tradBox, "trad"); syncCombo(ragaBox, "raga"); syncCombo(genreBox, "sgen");
    syncCombo(electroBox, "selec"); syncCombo(otherBox, "sother");

    // Rules: pick a tradition first (step 1), then styles (step 2). A raga switches all of that off.
    auto gate = [](Component& c, bool on) { c.setEnabled(on); c.setAlpha(on ? 1.0f : 0.4f); };
    gate(tradBox, !ragaOn);
    for (auto* c : { &genreBox, &electroBox, &otherBox }) gate(*c, s.trad > 0 && !ragaOn);
    gate(modeBox, !ragaOn && !modal);
    for (int i = 0; i < 7; ++i) gate(vibeBtn[i], step1 && !ragaOn);
    gate(generateBtn, step1); gate(newMelody, step1);

    for (int i = 0; i < 7; ++i) vibeBtn[i].setToggleState(i == s.vibe, dontSendNotification);
    const int plen = (int) fire::popularList(s).size();
    for (int i = 0; i < 3; ++i) {
        const bool vis = i < plen;
        popBtn[i].setVisible(vis);
        if (vis) {
            popBtn[i].setButtonText(chipText(s, fire::popularTokens(s, i)));
            popBtn[i].setToggleState(!ragaOn && s.source == 0 && i == s.prog % plen, dontSendNotification);
            gate(popBtn[i], step1);
        }
        const bool gv = i < (int) genHist.size();
        genBtn[i].setVisible(gv);
        if (gv) {
            auto t = s; t.gen = genHist[(size_t) i];
            genBtn[i].setButtonText(chipText(t, fire::generatedTokens(t)));
            genBtn[i].setToggleState((ragaOn || s.source == 1) && genHist[(size_t) i] == s.gen, dontSendNotification);
            gate(genBtn[i], step1);
        }
    }
    for (int i = 0; i < 3; ++i) {
        cTone[i].setToggleState(i == geti("csound"), dontSendNotification);
        mTone[i].setToggleState(i == geti("msound"), dontSendNotification);
    }

    const int hs = proc.historySize(), hp = proc.historyPos();
    for (int i = 0; i < 4; ++i) {
        const int idx = hs - 1 - i;
        histIdx[i] = idx;
        histBtn[i].setVisible(idx >= 0);
        if (idx >= 0) {
            histBtn[i].setButtonText(describe(proc.historyAt(idx)));
            histBtn[i].setToggleState(idx == hp, dontSendNotification);
        }
    }
    undoBtn.setEnabled(hp > 0);
    redoBtn.setEnabled(hp < hs - 1);

    proc.trackMelody(fire::melodyHash(fire::makeMelody(s, fire::buildSegs(s), (unsigned) geti("seed"))));

    const bool synced = proc.hostSynced.load();
    playBtn.setEnabled(!synced);
    bpm.setEnabled(!synced);
    repaint();
}

// ---------------------------------------------------------------- layout
void FireProgressEditor::resized() {
    auto r = getLocalBounds().reduced(24);
    r.removeFromTop(64);
    footer = r.removeFromBottom(36);
    r.removeFromBottom(10);
    auto left = r.removeFromLeft((r.getWidth() - 16) / 2);
    r.removeFromLeft(16);
    auto right = r;

    // ---- left column: style (steps 1 + 2 + raga), main chord, progressions, history
    cardStyle = left.removeFromTop(134);
    {
        auto c = cardStyle.reduced(14, 10); c.removeFromTop(18);
        auto lab1 = c.removeFromTop(14); auto row1 = c.removeFromTop(28); c.removeFromTop(4);
        auto lab2 = c.removeFromTop(14); auto row2 = c.removeFromTop(28);
        const int hw = (row1.getWidth() - 16) / 2;
        lblTrad = lab1.removeFromLeft(hw); lab1.removeFromLeft(16); lblRaga = lab1;
        tradBox.setBounds(row1.removeFromLeft(hw)); row1.removeFromLeft(16); ragaBox.setBounds(row1);
        const int tw = (row2.getWidth() - 16) / 3;
        lblGenre = lab2.removeFromLeft(tw); lab2.removeFromLeft(8); lblElec = lab2.removeFromLeft(tw); lab2.removeFromLeft(8); lblOther = lab2;
        genreBox.setBounds(row2.removeFromLeft(tw)); row2.removeFromLeft(8);
        electroBox.setBounds(row2.removeFromLeft(tw)); row2.removeFromLeft(8);
        otherBox.setBounds(row2);
    }
    left.removeFromTop(10);
    cardMain = left.removeFromTop(120);
    {
        auto c = cardMain.reduced(14, 10); c.removeFromTop(22);
        auto row = c.removeFromTop(30);
        rootBox.setBounds(row.removeFromLeft(86));  row.removeFromLeft(8);
        modeBox.setBounds(row.removeFromLeft(100)); row.removeFromLeft(8);
        barsBox.setBounds(row.removeFromLeft(96));  row.removeFromLeft(8);
        countBox.setBounds(row.removeFromLeft(110));
        c.removeFromTop(8);
        auto vr = c.removeFromTop(34);
        const int w = vr.getWidth() / 7;
        for (int i = 0; i < 7; ++i) vibeBtn[i].setBounds(vr.removeFromLeft(w).reduced(2, 0));
    }
    left.removeFromTop(10);
    cardProg = left.removeFromTop(178);
    {
        auto c = cardProg.reduced(14, 10);
        auto head = c.removeFromTop(26);
        auto colL = c.removeFromLeft((c.getWidth() - 12) / 2);
        c.removeFromLeft(12);
        auto colR = c;
        generateBtn.setBounds(head.removeFromRight(110).withSizeKeepingCentre(110, 24));
        for (int i = 0; i < 3; ++i) {
            popBtn[i].setBounds(colL.removeFromTop(40)); colL.removeFromTop(6);
            genBtn[i].setBounds(colR.removeFromTop(40)); colR.removeFromTop(6);
        }
    }
    left.removeFromTop(10);
    cardHist = left;
    {
        auto c = cardHist.reduced(14, 8);
        auto head = c.removeFromTop(24);
        listBtn.setBounds(head.removeFromRight(56)); head.removeFromRight(6);
        redoBtn.setBounds(head.removeFromRight(56)); head.removeFromRight(6);
        undoBtn.setBounds(head.removeFromRight(56));
        c.removeFromTop(4);
        const int w = c.getWidth() / 4;
        for (int i = 0; i < 4; ++i) histBtn[i].setBounds(c.removeFromLeft(w).reduced(2, 0));
    }

    // ---- right column: chord cards, melody, info
    chordArea = right.removeFromTop(84);
    right.removeFromTop(10);
    cardMel = right.removeFromTop(338);
    right.removeFromTop(10);
    cardInfo = right;
    {
        auto c = cardMel.reduced(14, 10); c.removeFromTop(22);
        auto rowB = c.removeFromBottom(34);
        padChords.setBounds(rowB.removeFromLeft((rowB.getWidth() - 8) / 2)); rowB.removeFromLeft(8);
        padMelody.setBounds(rowB);
        c.removeFromBottom(6);
        auto rowA = c.removeFromBottom(30);
        newMelody.setBounds(rowA.removeFromLeft(150)); rowA.removeFromLeft(8);
        chordsBtn.setBounds(rowA.removeFromLeft(90)); rowA.removeFromLeft(8);
        melodyBtn.setBounds(rowA.removeFromLeft(90));
        c.removeFromBottom(6);
        auto rowMT = c.removeFromBottom(28);
        c.removeFromBottom(4);
        auto rowCT = c.removeFromBottom(28);
        c.removeFromBottom(6);
        auto rowH = c.removeFromBottom(26);
        c.removeFromBottom(8);
        rollArea = c;
        lblMT = rowMT.removeFromLeft(110);
        for (int i = 0; i < 3; ++i) { mTone[i].setBounds(rowMT.removeFromLeft(84)); rowMT.removeFromLeft(6); }
        lblCT = rowCT.removeFromLeft(110);
        for (int i = 0; i < 3; ++i) { cTone[i].setBounds(rowCT.removeFromLeft(84)); rowCT.removeFromLeft(6); }
        lblH = rowH.removeFromLeft(110);
        humanSlider.setBounds(rowH.removeFromLeft(300));
    }

    // ---- footer
    auto f = footer;
    playBtn.setBounds(f.removeFromLeft(90).withSizeKeepingCentre(90, 30)); f.removeFromLeft(10);
    bpm.setBounds(f.removeFromLeft(300).withSizeKeepingCentre(300, 26));
}

// ---------------------------------------------------------------- painting
void FireProgressEditor::paintInfo(Graphics& g, const fire::Setup& s) {
    auto c = cardInfo.reduced(14, 10); c.removeFromTop(18);
    g.setColour(kText);
    if (!(s.trad > 0 || fire::isRaga(s))) {
        g.setFont(13.f);
        g.drawFittedText("Start with step 1: choose a world music tradition (or an Indian raga). Then pick a style in step 2 "
                         "and press Generate or New melody.", c, Justification::topLeft, 4);
        return;
    }
    if (fire::isRaga(s)) {
        const auto& r = fire::ragas()[(size_t) s.raga];
        String notes;
        for (int x : r.notes) notes += String(fire::SWARA[x % 12]) + " ";
        g.setFont(Font(15.f, Font::bold));
        g.drawText("Raga " + String(r.name) + "  -  " + String(r.mood), c.removeFromTop(22), Justification::centredLeft);
        g.setColour(kMut); g.setFont(12.f);
        g.drawText("Notes: " + notes + "   Vadi: " + String(fire::SWARA[r.vadi % 12]) + "   Samvadi: " + String(fire::SWARA[r.samvadi % 12]),
                   c.removeFromTop(18), Justification::centredLeft);
        g.drawFittedText("Raga mode is ON. Tradition, style, mode and vibe are switched off. Chords and melody use only this "
                         "raga's notes (Sa = " + String(fire::NOTE[s.root]) + ").", c, Justification::topLeft, 3);
        return;
    }
    const auto mx = fire::mixOf(s);
    String scale;
    for (int x : fire::melodyScale(s)) scale += String(fire::NOTE[(s.root + x) % 12]) + " ";
    g.setFont(Font(15.f, Font::bold));
    g.drawText(labelOf(s), c.removeFromTop(22), Justification::centredLeft);
    g.setColour(kMut); g.setFont(12.f);
    g.drawText("Scale: " + scale, c.removeFromTop(18), Justification::centredLeft);
    g.drawFittedText("Tempo " + String(mx.bpm) + " BPM  |  Chord rhythm: " + String(fire::patName(mx.pat)) + "  |  Chords: " +
                     String(fire::soundName(mx.valid ? mx.cs : geti("csound"))) + ", melody: " +
                     String(fire::soundName(mx.valid ? mx.ms : geti("msound"))), c, Justification::topLeft, 2);
}

void FireProgressEditor::paint(Graphics& g) {
    g.setGradientFill(ColourGradient(kBg2, 0.f, 0.f, kBg1, 0.f, (float) getHeight(), false));
    g.fillAll();

    const auto s = setup();
    const bool ragaOn = fire::isRaga(s);
    const auto segs = fire::buildSegs(s);
    const auto mel = fire::makeMelody(s, segs, (unsigned) geti("seed"));
    const int root = s.root, bars = s.bars, count = s.count;
    const double L = bars * 4.0, beat = proc.currentBeat.load();
    int cur = -1;
    if (beat >= 0.0) for (size_t i = 0; i < segs.size(); ++i) if (beat >= segs[i].start) cur = (int) i;

    // header: title + DAW sync pill
    g.setGradientFill(ColourGradient(kFire, 24.f, 18.f, kGold, 250.f, 18.f, false));
    g.setFont(Font(34.f, Font::bold));
    g.drawText("Fire Progress", 24, 14, 330, 44, Justification::centredLeft);
    {
        const bool synced = proc.hostSynced.load();
        const String t = synced ? "DAW sync  " + String(proc.hostBpm.load(), 1) + " BPM"
                                : "Standalone clock  " + String(geti("bpm")) + " BPM";
        Rectangle<float> pill((float) getWidth() - 24.f - 250.f, 22.f, 250.f, 30.f);
        g.setColour(kCard2); g.fillRoundedRectangle(pill, 15.f);
        g.setColour(synced ? kGold : kMut);
        g.fillEllipse(pill.getX() + 12.f, pill.getCentreY() - 4.f, 8.f, 8.f);
        g.setColour(kText); g.setFont(Font(13.f, Font::bold));
        g.drawText(t, pill.withTrimmedLeft(28.f), Justification::centredLeft);
    }

    // cards
    auto card = [&](Rectangle<int> r, const String& label) {
        g.setColour(kCard); g.fillRoundedRectangle(r.toFloat(), 14.f);
        g.setColour(kLine); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 14.f, 1.f);
        g.setColour(kMut); g.setFont(Font(11.f, Font::bold));
        g.drawText(label, r.getX() + 14, r.getY() + 8, 460, 16, Justification::centredLeft);
    };
    card(cardStyle, "STEP 1: WORLD MUSIC   >   STEP 2: STYLE   |   OR PICK AN INDIAN RAGA");
    card(cardMain, ragaOn ? "KEY (Sa),  PATTERN SIZE  &  CHORDS" : "MAIN CHORD,  PATTERN SIZE  &  VIBE");
    card(cardProg, ragaOn ? "POPULAR  (off in raga mode)" : "POPULAR");
    card(cardMel, "MELODY");
    card(cardHist, "HISTORY   (Ctrl+Z undo, Ctrl+Shift+Z redo)");
    card(cardInfo, "WHAT YOU ARE PLAYING");
    paintInfo(g, s);

    g.setColour(kMut); g.setFont(Font(10.f, Font::bold));
    g.drawText("1  WORLD MUSIC (choose first)", lblTrad, Justification::centredLeft);
    g.drawText("INDIAN RAGA (switches the rest off)", lblRaga, Justification::centredLeft);
    g.drawText("2  POPULAR GENRE", lblGenre, Justification::centredLeft);
    g.drawText("2  ELECTRONIC", lblElec, Justification::centredLeft);
    g.drawText("2  OTHER STYLE", lblOther, Justification::centredLeft);
    g.setFont(Font(11.f, Font::bold));
    g.drawText("CHORD SOUND", lblCT, Justification::centredLeft);
    g.drawText("MELODY SOUND", lblMT, Justification::centredLeft);
    g.drawText("HUMANIZE", lblH, Justification::centredLeft);
    {
        auto c = cardProg.reduced(14, 10);
        auto colL = c.removeFromLeft((c.getWidth() - 12) / 2);
        c.removeFromLeft(12);
        g.drawText("GENERATED", c.getX(), cardProg.getY() + 8, 200, 16, Justification::centredLeft);
        g.setFont(12.f);
        if (ragaOn)
            g.drawFittedText("Raga mode: Popular progressions are off. Use Generate for chords built only from the raga's notes.",
                             colL.withTrimmedTop(18), Justification::topLeft, 4);
        if (genHist.empty())
            g.drawFittedText("Press Generate to build new progressions from your main chord (or from the raga).",
                             c.withTrimmedTop(30), Justification::topLeft, 3);
    }

    // chord cards: width follows how long each chord lasts
    for (size_t i = 0; i < segs.size(); ++i) {
        const float x0 = (float) chordArea.getX() + (float) (segs[i].start / L) * (float) chordArea.getWidth();
        const float x1 = (float) chordArea.getX() + (float) ((segs[i].start + segs[i].len) / L) * (float) chordArea.getWidth();
        auto r = Rectangle<float>(x0 + 2.f, (float) chordArea.getY(), x1 - x0 - 4.f, (float) chordArea.getHeight());
        const bool lit = (int) i == cur;
        if (lit) g.setGradientFill(ColourGradient(kFire, r.getX(), r.getY(), kGold, r.getRight(), r.getBottom(), false));
        else     g.setColour(kCard);
        g.fillRoundedRectangle(r, 12.f);
        if (!lit) { g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f); }
        g.setColour(lit ? kBg1 : kText);
        g.setFont(Font(count <= 4 ? 26.f : (count <= 6 ? 21.f : 16.f), Font::bold));
        g.drawFittedText(String(fire::chordName(root, segs[i].chord)), r.withTrimmedBottom(26.f).toNearestInt(), Justification::centred, 1);
        g.setColour(lit ? kBg1 : kMut);
        g.setFont(11.f);
        g.drawFittedText(String(fire::chordNotes(root, segs[i].chord)), r.removeFromBottom(30.f).toNearestInt(), Justification::centred, 1);
    }

    // melody status badge: new vs used before
    {
        const bool rep = proc.melodyIsRepeat();
        const String t = (rep ? "USED BEFORE" : "NEW MELODY") + String("  -  ") + String(proc.usedCount()) + " heard";
        Rectangle<float> b((float) cardMel.getRight() - 14.f - 190.f, (float) cardMel.getY() + 6.f, 190.f, 20.f);
        if (rep) { g.setColour(kGold); g.drawRoundedRectangle(b, 10.f, 1.5f); g.setColour(kGold); }
        else     { g.setGradientFill(ColourGradient(kFire, b.getX(), b.getY(), kGold, b.getRight(), b.getBottom(), false)); g.fillRoundedRectangle(b, 10.f); g.setColour(kBg1); }
        g.setFont(Font(11.f, Font::bold));
        g.drawText(t, b, Justification::centred);
    }

    // piano roll with chord names and a moving playhead
    {
        auto rr = rollArea.toFloat();
        g.setColour(kBg1); g.fillRoundedRectangle(rr, 8.f);
        const int steps = bars * 8;
        const float sw = rr.getWidth() / (float) steps, nh = rr.getHeight() / 26.f;
        g.setColour(kLine);
        for (int b = 1; b < bars; ++b) g.fillRect(rr.getX() + (float) b * 8.f * sw, rr.getY(), 1.f, rr.getHeight());
        if (cur >= 0) {
            g.setColour(kFire.withAlpha(0.2f));
            g.fillRect(rr.getX() + (float) (segs[(size_t) cur].start / L) * rr.getWidth(), rr.getY(),
                       (float) (segs[(size_t) cur].len / L) * rr.getWidth(), rr.getHeight());
        }
        g.setFont(Font(10.f, Font::bold));
        for (size_t i = 0; i < segs.size(); ++i) {
            const float x = rr.getX() + (float) (segs[i].start / L) * rr.getWidth();
            if (segs[i].start > 0) { g.setColour(kMut.withAlpha(0.55f)); g.fillRect(x, rr.getY(), 1.5f, rr.getHeight()); }
            g.setColour(kMut);
            g.drawText(String(fire::chordName(root, segs[i].chord)), (int) x + 3, (int) rr.getY() + 1, 60, 12, Justification::centredLeft);
        }
        g.setColour(kGold);
        for (auto& n : mel) {
            const float y = jmax(rr.getY(), rr.getBottom() - (float) (n.midi - 58) * nh);
            g.fillRoundedRectangle(rr.getX() + (float) n.step * sw + 1.f, y, jmax((float) n.len * sw - 2.f, 4.f), nh - 1.f, 2.f);
        }
        if (beat >= 0.0) {
            g.setColour(kText);
            g.fillRect(rr.getX() + (float) (beat / L) * rr.getWidth() - 1.f, rr.getY(), 2.f, rr.getHeight());
        }
    }

    // footer text
    g.setColour(kMut); g.setFont(12.f);
    const String foot = statusMsg.isNotEmpty() ? statusMsg
        : (proc.hostSynced.load() ? String("Following the DAW tempo and transport.")
                                  : String("Standalone: press Play. In a DAW it follows the host tempo automatically."));
    g.drawFittedText(foot, footer.withTrimmedLeft(420), Justification::centredRight, 2);
}
