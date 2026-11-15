#pragma once
// 125A hardware rendering primitives. Ordinary VSTGUI CKnob interaction is
// retained; only the faceplate is drawn here. No bitmap files, JUCE or DAW
// extensions are needed to draw the black gloss controls.
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cview.h"
#include "vstgui/contrib/keyboardview.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace a125ui {

// A sober, black-glass dial with tangible brushed-metal rim.
// Deliberately retain CKnob's original hit targets, automation and drag law.
class BlackGlossKnob final : public VSTGUI::CKnob {
public:
    explicit BlackGlossKnob(const VSTGUI::CKnob& original)
        : VSTGUI::CKnob(original.getViewSize(),original.getListener(),
                        original.getTag(),nullptr,nullptr) {
        setMin(original.getMin());
        setMax(original.getMax());
        setValue(original.getValue());
        setStartAngle(original.getStartAngle());
        setRangeAngle(original.getRangeAngle());
        setMouseEnabled(original.getMouseEnabled());
        setWheelInc(original.getWheelInc());
    }
    void draw(VSTGUI::CDrawContext* dc) override {
        if(!dc)return;
        dc->setDrawMode(VSTGUI::kAntiAliasing|VSTGUI::kNonIntegralMode);
        const auto bounds=getViewSize();
        const double diameter=std::min(bounds.getWidth(),bounds.getHeight());
        const auto center=bounds.getCenter();
        auto ring=[&](double radius,VSTGUI::CColor fill,
                      VSTGUI::CColor border,double line=1.) {
            VSTGUI::CRect r(center.x-radius,center.y-radius,
                            center.x+radius,center.y+radius);
            dc->setFillColor(fill);
            dc->setFrameColor(border);
            dc->setLineWidth(line);
            dc->drawEllipse(r,VSTGUI::kDrawFilledAndStroked);
        };
        // Dark housing, two fine metal edges, then four dark lacquer
        // layers: no aluminum disks or conspicuous white central spots.
        const double radius=diameter*.475;
        ring(radius,     {19,22,28,255},{86,95,103,255},1.35);
        ring(radius-2.0, {54,62,69,255},{124,132,138,255},.65);
        ring(radius-4.4, {7,10,15,255}, {14,19,24,255},1.15);
        ring(radius-7.2, {12,16,23,255},{63,72,81,255},.9);
        ring(radius-10., {16,23,33,255},{7,10,13,255},.8);
        // High-gloss upper arc: subtle cold reflection, retained black
        // faceplate and a short, precise specular rim.
        dc->setLineWidth(2.4);
        dc->setFrameColor({112,139,155,255});
        dc->drawLine({center.x-radius*.58,center.y-radius*.64},
                     {center.x-radius*.10,center.y-radius*.81});
        dc->setLineWidth(1.35);
        dc->setFrameColor({207,227,235,255});
        dc->drawLine({center.x-radius*.41,center.y-radius*.72},
                     {center.x-radius*.14,center.y-radius*.78});
        // Eleven restrained, meaningful 0..100% ticks around the face.
        const double fraction=std::clamp(
            static_cast<double>(getValueNormalized()),0.,1.);
        constexpr double pi=3.14159265358979323846;
        for(int i=0;i<=10;++i){
            const double x=static_cast<double>(i)/10.;
            const double angle=(-225.+270.*x)*pi/180.;
            const double cx=std::cos(angle),cy=std::sin(angle);
            const double r0=diameter*(i%5==0?.393:.412);
            const double r1=diameter*.466;
            dc->setLineWidth(i%5==0?1.65:.9);
            dc->setFrameColor(x<=fraction?
                VSTGUI::CColor{224,167,79,255}:
                VSTGUI::CColor{104,114,124,255});
            dc->drawLine({center.x+cx*r0,center.y+cy*r0},
                         {center.x+cx*r1,center.y+cy*r1});
        }
        // Short amber index on the actual parameter angle; no dummy dial.
        const double angle=(-225.+270.*fraction)*pi/180.;
        const double vx=std::cos(angle),vy=std::sin(angle);
        const double inner=diameter*.16,outer=diameter*.325;
        dc->setFrameColor({251,192,92,255});
        dc->setLineWidth(3.);
        dc->drawLine({center.x+vx*inner,center.y+vy*inner},
                     {center.x+vx*outer,center.y+vy*outer});
        dc->setFrameColor({250,221,169,255});
        dc->setLineWidth(1.);
        dc->drawLine({center.x+vx*inner,center.y+vy*inner},
                     {center.x+vx*outer,center.y+vy*outer});
        setDirty(false);
    }
};

// Keep Steinberg's MIDI/key-hit mechanics intact. A narrow gold index strip
// marks the recorded root samples; the built-in keys remain fully clickable.
class ZoneKeyboardView final : public VSTGUI::KeyboardView {
public:
    explicit ZoneKeyboardView(const VSTGUI::KeyboardView& original) {
        setViewSize(original.getViewSize());
        setKeyRange(original.getKeyRangeStart(),original.getNumKeys());
        setWhiteKeyWidth(original.getWhiteKeyWidth());
        setBlackKeyWidth(original.getBlackKeyWidth());
        setBlackKeyHeight(original.getBlackKeyHeight());
        setLineWidth(original.getLineWidth());
        setFrameColor(original.getFrameColor());
        setFontColor(original.getFontColor());
        setWhiteKeyColor(original.getWhiteKeyColor());
        setBlackKeyColor(original.getBlackKeyColor());
        setWhiteKeyPressedColor(original.getWhiteKeyPressedColor());
        setBlackKeyPressedColor(original.getBlackKeyPressedColor());
        // Use Steinberg's actual octave labels and MIDI hit-testing.
        // The default SDK keyboard omits the octave names and looks flat.
        setDrawNoteText(true);
        setFontColor({45,52,62,255});
        setMouseEnabled(original.getMouseEnabled());
    }
    void setRoots(const std::array<bool,128>& mask) {
        if(roots_==mask)return;
        roots_=mask;
        invalid();
    }
    void drawRect(VSTGUI::CDrawContext* dc,
                  const VSTGUI::CRect& dirty) override {
        // The original KeyboardView owns all note hit-testing, hover,
        // MIDI press state, pressed-key color and labels.
        VSTGUI::KeyboardView::drawRect(dc,dirty);
        dc->setDrawMode(VSTGUI::kAntiAliasing|VSTGUI::kNonIntegralMode);
        const int first=getKeyRangeStart();
        const int last=std::min(127,first+getNumKeys());
        // Delicately beveled white-key fronts (below the black-key area).
        // These surfaces don't obscure Steinberg's pressed-note colors.
        for(int note=first;note<=last;++note){
            if(!isWhiteKey(note))continue;
            auto r=getNoteRect(static_cast<NoteIndex>(note));
            if(r.isEmpty() || !dirty.rectOverlap(r))continue;
            auto lip=r;
            lip.left+=1.;lip.right-=1.;
            lip.top=lip.bottom-7.;
            dc->setFillColor({169,178,187,100});
            dc->drawRect(lip,VSTGUI::kDrawFilled);
            lip.top=lip.bottom-4.;
            dc->setFillColor({225,229,233,190});
            dc->drawRect(lip,VSTGUI::kDrawFilled);
            dc->setFrameColor({251,253,255,160});
            dc->setLineWidth(1.35);
            dc->drawLine({r.left+2.,r.bottom-18.},
                         {r.left+2.,r.bottom-7.});
        }
        // Lacquered black keycaps with narrow specular reflections. All
        // highlights are clipped by each key's actual geometry.
        for(int note=first;note<=last;++note){
            if(isWhiteKey(note))continue;
            auto r=getNoteRect(static_cast<NoteIndex>(note));
            if(r.isEmpty() || !dirty.rectOverlap(r))continue;
            auto shine=r;
            shine.inset(2.5,2.5);
            shine.bottom=shine.top+8.;
            dc->setFillColor({56,66,76,255});
            dc->drawRect(shine,VSTGUI::kDrawFilled);
            shine.top+=1.5;shine.bottom=shine.top+2.;
            dc->setFillColor({102,118,130,255});
            dc->drawRect(shine,VSTGUI::kDrawFilled);
            dc->setFrameColor({169,186,197,255});
            dc->setLineWidth(1.0);
            dc->drawLine({r.left+3.,r.top+5.},
                         {r.left+3.,r.bottom-12.});
            dc->setFrameColor({5,8,12,255});
            dc->drawLine({r.left+1.5,r.bottom-2.},
                         {r.right-1.5,r.bottom-2.});
        }
        // Index marks signal real sampled roots; never show a fake light.
        dc->setFillColor({225,168,71,255});
        for(int note=first;note<=last;++note){
            if(!roots_[static_cast<std::size_t>(note)])continue;
            auto r=getNoteRect(static_cast<NoteIndex>(note));
            if(r.isEmpty() || !dirty.rectOverlap(r))continue;
            r.top+=1.;r.bottom=r.top+4.;
            r.left+=1.5;r.right-=1.5;
            dc->drawRect(r,VSTGUI::kDrawFilled);
        }
    }
private:
    std::array<bool,128> roots_{};
};

// A factual PCM peak envelope drawn from the selected sample. Its scale,
// center line and grid are hardware-like, but the displayed waveform is real.
class WaveformView final : public VSTGUI::CView {
public:
    static constexpr std::size_t Points=192;
    explicit WaveformView(const VSTGUI::CRect& r):CView(r){}
    void setWave(const std::array<float,Points>& data,bool hasData){
        if(values_==data && valid_==hasData)return;
        values_=data;valid_=hasData;
        invalid();
    }
    void draw(VSTGUI::CDrawContext* ctx) override {
        if(!ctx)return;
        const auto r=getViewSize();
        ctx->setDrawMode(VSTGUI::kAntiAliasing|VSTGUI::kNonIntegralMode);
        ctx->setFillColor({11,21,33,255});
        ctx->setFrameColor({105,119,135,255});
        ctx->setLineWidth(1.);
        ctx->drawRect(r,VSTGUI::kDrawFilledAndStroked);

        // Fixed measurement grid, not a fake waveform or fictional meters.
        const auto left=r.left+7.;
        const auto right=r.right-7.;
        const auto width=right-left;
        const auto top=r.top+7.;
        const auto bottom=r.bottom-7.;
        const auto height=bottom-top;
        const auto center=(top+bottom)*.5;
        ctx->setFrameColor({34,52,69,255});
        ctx->setLineWidth(1.);
        for(int i=1;i<12;++i){
            const double x=left+width*static_cast<double>(i)/12.;
            ctx->drawLine({x,top},{x,bottom});
        }
        for(int j=1;j<4;++j){
            const double y=top+height*static_cast<double>(j)/4.;
            ctx->drawLine({left,y},{right,y});
        }
        ctx->setFrameColor({81,112,134,255});
        ctx->drawLine({left,center},{right,center});
        if(valid_){
            const double amplitude=height*.45;
            // Soft blue trace beneath a sharp light trace: sample peaks
            // remain visible without smearing attacks or adding fake peaks.
            for(int pass=0;pass<2;++pass){
                ctx->setFrameColor(pass==0?
                    VSTGUI::CColor{52,110,160,255}:
                    VSTGUI::CColor{166,213,247,255});
                ctx->setLineWidth(pass==0?3.:1.25);
                for(std::size_t i=0;i<Points;++i){
                    const auto x=left+static_cast<double>(i)*width/(Points-1);
                    const auto y=amplitude*std::clamp(
                        static_cast<double>(values_[i]),0.,1.);
                    ctx->drawLine({x,center-y},{x,center+y});
                }
            }
        }
        setDirty(false);
    }
private:
    std::array<float,Points> values_{};
    bool valid_=false;
};

} // namespace a125ui
