#pragma once
// 125A hardware rendering primitives. Ordinary VSTGUI CKnob interaction is
// retained; only the faceplate is drawn here. No bitmap files, JUCE or DAW
// extensions are needed to draw the silver metal dials.
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cview.h"
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
        // A bright upper-left reflection and a fixed dark center make the
        // dial look metallic even before the final photographic GUI skins.
        auto spot=r;
        spot.inset(9.,9.);
        spot.offset(-2.,-2.);
        circle(spot,{224,228,233,255},{199,206,211,255},.2);
        const auto center=getViewSize().getCenter();
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
        ctx->setFillColor({17,32,29,255});
        ctx->setFrameColor({63,88,78,255});
        ctx->setLineWidth(1.);
        ctx->drawRect(r,VSTGUI::kDrawFilledAndStroked);
        const auto center=(r.top+r.bottom)/2.;
        ctx->setFrameColor({57,88,72,255});
        ctx->drawLine({r.left+6.,center},{r.right-6.,center});
        if(valid_){
            ctx->setFrameColor({188,230,177,255});
            ctx->setLineWidth(1.35);
            const auto width=r.getWidth()-12.;
            const auto height=r.getHeight()*.43;
            for(std::size_t i=0;i<Points;++i){
                const auto x=r.left+6.+static_cast<double>(i)*width/(Points-1);
                const auto y=height*std::clamp(
                    static_cast<double>(values_[i]),0.,1.);
                ctx->drawLine({x,center-y},{x,center+y});
            }
        }
        setDirty(false);
    }
private:
    std::array<float,Points> values_{};
    bool valid_=false;
};

} // namespace a125ui
