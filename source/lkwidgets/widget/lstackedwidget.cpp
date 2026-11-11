#include "lstackedwidget.h"

LK_NAMESPACE_BEGIN

LStackedWidget::LStackedWidget(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f):LWidget(parent, f) {
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::DefaultType);
}

int LStackedWidget::addWidget(std::shared_ptr<LWidget> w){
    if(m_layout != nullptr){
        return m_layout->addWidget(w);
    }
    return -1;
}

int LStackedWidget::insertWidget(int index, std::shared_ptr<LWidget> w)
{
    if(m_layout != nullptr){
        return m_layout->addWidget(w, LAlignment::None, index);
    }
    return -1;
}

void LStackedWidget::removeWidget(std::shared_ptr<LWidget> w)
{
    if(m_layout != nullptr){
        int pos = m_layout->widgetPos(w);
        if(pos >=0){
            m_layout->removeItem(pos);
        }
    }
}

std::shared_ptr<LWidget> LStackedWidget::currentWidget() const
{
    std::shared_ptr<LStackedLayout> layout = std::dynamic_pointer_cast<LStackedLayout>(m_layout);
    if(layout != nullptr){
        return layout->currentWidget();
    }
    return nullptr;
}

int LStackedWidget::currentIndex() const
{
    std::shared_ptr<LStackedLayout> layout = std::dynamic_pointer_cast<LStackedLayout>(m_layout);
    if(layout != nullptr){
        return layout->currentIndex();
    }
    return -1;
}

int LStackedWidget::indexOf(const std::shared_ptr<LWidget> w) const
{
    std::shared_ptr<LStackedLayout> layout = std::dynamic_pointer_cast<LStackedLayout>(m_layout);
    if(layout != nullptr){
        return layout->widgetPos(w);
    }
    return -1;
}

std::shared_ptr<LWidget> LStackedWidget::widget(int index) const
{
    if(m_layout != nullptr){
        return m_layout->widget(index);
    }
    return nullptr;
}

int LStackedWidget::count() const
{
    if(m_layout != nullptr){
        return m_layout->count();
    }
    return 0;
}

void LStackedWidget::setCurrentIndex(int index)
{
    std::shared_ptr<LStackedLayout> layout = std::dynamic_pointer_cast<LStackedLayout>(m_layout);
    if(layout != nullptr){
        return layout->setCurrentIndex(index);
    }
}

void LStackedWidget::setCurrentWidget(std::shared_ptr<LWidget> w)
{
    std::shared_ptr<LStackedLayout> layout = std::dynamic_pointer_cast<LStackedLayout>(m_layout);
    if(layout != nullptr){
        return layout->setCurrentIndex(layout->widgetPos(w));
    }
}

std::shared_ptr<LStackedLayout> LStackedWidget::stackedLayout()
{
    return std::dynamic_pointer_cast<LStackedLayout>(m_layout);
}

void LStackedWidget::initSpecific()
{
    std::shared_ptr<LLayout> layout = std::make_shared<LStackedLayout>();
    setLayout(layout);
}


LK_NAMESPACE_END
