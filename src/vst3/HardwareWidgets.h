#pragma once
// 125A hardware rendering primitives. Ordinary VSTGUI CKnob interaction is
// retained; only the faceplate is drawn here. No bitmap files, JUCE or DAW
// extensions are needed to draw the silver metal dials.
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cview.h"
#include "vstgui/contrib/keyboardview.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace a125ui {

class SilverKnob final : public VSTGUI::CKnob {
public:
    explicit SilverKnob(const VSTGUI::CKnob& original)
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
    void draw(VSTGUI::CDrawContext* ctx) override {
        if(!ctx)return;
        auto circle=[&](VSTGUI::CRect r,VSTGUI::CColor fill,
                        VSTGUI::CColor outline,double width=1.) {
            ctx->setFillColor(fill);
            ctx->setFrameColor(outline);
            ctx->setLineWidth(width);
            ctx->drawEllipse(r,VSTGUI::kDrawFilledAndStroked);
        };
        ctx->setDrawMode(VSTGUI::kAntiAliasing|VSTGUI::kNonIntegralMode);
        auto r=getViewSize();
        const auto w=std::min(r.getWidth(),r.getHeight());
        r.inset((r.getWidth()-w)/2.+2.,(r.getHeight()-w)/2.+2.);
        circle(r,{17,19,24,255},{91,96,104,255},1.4);
        r.inset(3.,3.);
        circle(r,{60,64,71,255},{10,13,16,255},1.);
        r.inset(2.,2.);
        circle(r,{192,197,201,255},{240,243,247,255},1.3);
        r.inset(3.,3.);
        circle(r,{146,151,157,255},{95,100,108,255},.7);
        r.inset(3.,3.);
        circle(r,{193,198,202,255},{102,109,116,255},.8);
        r.inset(4.,4.);
        circle(r,{173,180,186,255},{227,230,234,255},.6);
        // Fine engraved radial scale. Eleven marks use the same arc as
        // CKnob: the end stops are real 0% / 100%, never decorative.
        const auto center=getViewSize().getCenter();
        const auto current=std::clamp(getValueNormalized(),0.f,1.f);
        for(int mark=0;mark<=10;++mark){
            const double fraction=static_cast<double>(mark)/10.;
            const double angle=(-225.+270.*fraction)*
                               3.14159265358979323846/180.;
            const double inner=w*(mark%5==0?.387:.409);
            const double outer=w*.462;
            const auto x=std::cos(angle);
            const auto y=std::sin(angle);
            ctx->setLineWidth(mark%5==0?1.8:1.2);
            ctx->setFrameColor(fraction<=current?
                VSTGUI::CColor{229,174,89,255}:
                VSTGUI::CColor{113,120,130,255});
            ctx->drawLine({center.x+x*inner,center.y+y*inner},
                          {center.x+x*outer,center.y+y*outer});
        }
        // Highlight is part of the dial, not a painted-on static image.
        auto spot=r;
        spot.inset(9.,9.);
        spot.offset(-2.,-2.);
        circle(spot,{224,228,233,255},{199,206,211,255},.2);
        const double angle=(-225.+270.*getValueNormalized())*
                           3.14159265358979323846/180.;
        const double radius=w*.27;
        VSTGUI::CPoint from(center.x+std::cos(angle)*radius*.30,
                            center.y+std::sin(angle)*radius*.30);
        VSTGUI::CPoint to(center.x+std::cos(angle)*radius,
                          center.y+std::sin(angle)*radius);
        ctx->setLineStyle(VSTGUI::kLineSolid);
        ctx->setLineWidth(std::max(2.,w*.060));
        ctx->setFrameColor({35,40,48,255});
        ctx->drawLine(from,to);
        from.offset(-.8,-.8);to.offset(-.8,-.8);
        ctx->setLineWidth(std::max(1.,w*.026));
        ctx->setFrameColor({245,247,250,255});
        ctx->drawLine(from,to);
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
        setDrawNoteText(original.getDrawNoteText());
        setMouseEnabled(original.getMouseEnabled());
    }
    void setRoots(const std::array<bool,128>& mask) {
        if(roots_==mask)return;
        roots_=mask;
        invalid();
    }
    void drawRect(VSTGUI::CDrawContext* dc,
                  const VSTGUI::CRect& dirty) override {
        VSTGUI::KeyboardView::drawRect(dc,dirty);
        dc->setFillColor({206,170,105,255});
        for(int note=getKeyRangeStart();
            note<=getKeyRangeStart()+getNumKeys() && note<128;++note) {
            if(!roots_[static_cast<std::size_t>(note)])continue;
            auto rect=getNoteRect(static_cast<NoteIndex>(note));
            if(rect.isEmpty() || !dirty.rectOverlap(rect))continue;
            rect.top+=1.;rect.bottom=rect.top+4.;
            dc->drawRect(rect,VSTGUI::kDrawFilled);
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
