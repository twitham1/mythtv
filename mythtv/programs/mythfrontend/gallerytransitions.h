//! \file
//! \brief Provides transitions for slideshows

#ifndef GALLERYTRANSITIONS_H
#define GALLERYTRANSITIONS_H

#include "galleryslide.h"


//! Available transitions
enum ImageTransitionType {
    // First transition must be 0 and useable by all painters
    kNoTransition      = 0,
    kRandomTransition  = 1,
    kBlendTransition   = 2,
    kTwistTransition   = 3,
    kSlideTransition   = 4,
    kZoomTransition    = 5,
    kSpinTransition    = 6,
    kLastTransSentinel = 7  // Must be last
};


//! Base class of an animated transition that can be accelerated & reversed
class Transition : public QObject
{
    Q_OBJECT
public:
    explicit Transition(QString name);
    virtual ~Transition()              = default;

    virtual void Start(Slide &from, Slide &to, bool forwards, float speed = 1.0);
    virtual void SetSpeed(float)       {}
    virtual void Pulse(int interval)   = 0;
    virtual void Initialise()          {}
    virtual void Finalise()            {}

protected slots:
    virtual void Finished();

signals:
    void         finished();

protected:
    //! Seconds transition will last
    int m_duration;
    //! The image currently displayed, which will be replaced
    //! (whatever the transition direction)
    Slide *m_old;
    //! The new image that will replace the current one
    //! (whatever the transition direction)
    Slide *m_new;
    // Transitions play forwards or backwards. Symmetric transitions can
    // define a one-way transition in terms of "prev" & "next" (as in
    // position rather than time). The reverse transition can then be
    // achieved by playing it backwards.
    // When played forwards next replaces prev, ie. prev = old, next = new
    // When played backwards prev replaces next, ie. prev = new, next = old
    //! The image occurring earlier in the slideshow sequence
    Slide *m_prev;
    //! The image occurring later in the slideshow sequence
    Slide *m_next;
};


typedef QMap<int, Transition*> TransitionMap;


//! Switches images instantly with no effects
class TransitionNone : public Transition
{
public:
    TransitionNone() : Transition("None") {}
    virtual void Start(Slide &from, Slide &to,
                       bool forwards, float speed = 1.0);
    virtual void Pulse(int) {}
};


//! Abstract class implementing sequential & parallel animations
class GroupTransition : public Transition
{
public:
    GroupTransition(GroupAnimation *animation, QString name);
    virtual ~GroupTransition();
    virtual void Start(Slide &from, Slide &to,
                       bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed);
    virtual void Pulse(int interval);
    virtual void Initialise()  = 0;
    virtual void Finalise()    = 0;

protected:
    GroupAnimation *m_animation;
};


//! Image blends into the next by fading
class TransitionBlend : public GroupTransition
{
public:
    TransitionBlend() : GroupTransition(new ParallelAnimation(), 
                                        Transition::tr("Blend")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Image twists into the next
class TransitionTwist : public GroupTransition
{
public:
    TransitionTwist() : GroupTransition(new SequentialAnimation(), 
                                        Transition::tr("Twist")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Slide on from right, then off to left
class TransitionSlide : public GroupTransition
{
public:
    TransitionSlide() : GroupTransition(new ParallelAnimation(), 
                                        Transition::tr("Slide")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Zooms from right, then away to left
class TransitionZoom : public GroupTransition
{
public:
    TransitionZoom() : GroupTransition(new ParallelAnimation(), 
                                       Transition::tr("Zoom")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Images blend whilst rotating
class TransitionSpin : public TransitionBlend
{
public:
    TransitionSpin() : TransitionBlend() 
    { setObjectName(Transition::tr("Spin")); }
    virtual void Initialise();
    virtual void Finalise();
};


//! Invokes random transitions
class TransitionRandom : public Transition
{
    Q_OBJECT
public:
    explicit TransitionRandom(QList<Transition*> peers)
        : Transition(Transition::tr("Random")), m_peers(peers), m_current(nullptr) {}
    virtual void Start(Slide &from, Slide &to, bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed) { if (m_current) m_current->SetSpeed(speed); }
    virtual void Pulse(int interval)   { if (m_current) m_current->Pulse(interval); }
    virtual void Initialise()          { if (m_current) m_current->Initialise(); }
    virtual void Finalise()            { if (m_current) m_current->Finalise(); }

protected slots:
    void Finished();

protected:
    //! Set of distinct transitions
    QList<Transition*> m_peers;
    //! Selected transition
    Transition        *m_current;
};


//! Manages transitions available to s psinter
class TransitionRegistry
{
public:
    explicit TransitionRegistry(bool includeAnimations);
    ~TransitionRegistry()    { qDeleteAll(m_map); }
    const TransitionMap GetAll() const { return m_map; }
    Transition &Select(int setting);

    //! A transition that updates instantly
    TransitionNone m_immediate;

private:
    //! All possible transitions
    TransitionMap m_map;
};


#endif // GALLERYTRANSITIONS_H
