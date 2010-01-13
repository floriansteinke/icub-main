/**
 * \defgroup affActionPrimitives affActionPrimitives
 *  
 * @ingroup icub_libraries 
 *  
 * Definition of primitive actions for dealing with affordances 
 * and more. 
 *
 * \author Ugo Pattacini
 *
 * \section intro_sec Description
 *
 * The library relies on the Yarp Cartesian Interface and 
 * provides the user a collection of action primitives in task 
 * space and joint space along with an easy way to combine them
 * together forming higher level actions (e.g. grasp(), tap(), 
 * �) in order to eventually execute more sophisticated tasks 
 * without reference to the motion control details. 
 *  
 * \image html affActionPrimitives.jpg 
 *  
 * Central to the library's implementation is the concept of 
 * \b action. An action is a "request" for an execution of three 
 * different tasks according to its internal selector: 
 *  
 * - 1 It can ask the system to wait for a specified time 
 * interval;
 *  
 * - 2 It can ask to steer the arm to a specified pose, hence 
 * performing a motion in the task space;
 *  
 * - 3 It can command the execution of some predefined 
 * fingers sequences in the joint space identified by a tag. 
 *  
 * Besides, there exists the possibility of generating one 
 * action for the execution of a task of type 2 simultaneously 
 * to a task of type 3. 
 *  
 * Moreover, whenever an action is produced from within the code 
 * the corresponding request item is pushed at the bottom of 
 * <b>actions queue</b>. Therefore, user can identify suitable 
 * fingers movements in the joint space, associate proper 
 * grasping 3d points together with hand posture and finally 
 * execute the grasping task as a harmonious combination of a 
 * reaching movement and fingers actuations, complying with the 
 * time requirements of the synchronized sequence. 
 *  
 * To detect contacts among fingers and objects the \ref 
 * icub_graspDetector module is employed.
 */ 

#ifndef __AFFACTIONPRIMITIVES_H__
#define __AFFACTIONPRIMITIVES_H__

#include <yarp/os/RateThread.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Bottle.h>
#include <yarp/os/Semaphore.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/CartesianControl.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/sig/Vector.h>

#include <string>
#include <deque>
#include <set>
#include <map>

#define ACTIONPRIM_DISABLE_EXECTIME    -1

/**
* \ingroup affActionPrimitives
*
* The base class defining actions. 
*  
* It allows to execute arm (in task-space, e.g. reach()) and 
* hand (in joint-space) primitive actions and to combine them in 
* the actions queue. 
*/
class affActionPrimitives : public yarp::os::RateThread
{
protected:
    std::string local;
    std::string part;

    yarp::dev::PolyDriver        *polyHand;
    yarp::dev::PolyDriver        *polyCart;
    yarp::dev::IEncoders         *encCtrl;
    yarp::dev::IPositionControl  *posCtrl;
    yarp::dev::ICartesianControl *cartCtrl;

    yarp::os::BufferedPort<yarp::os::Bottle> graspDetectionPort;

    yarp::os::Semaphore *mutex;
    void                *motionDoneEvent;

    bool armMoveDone;
    bool handMoveDone;
    bool latchArmMoveDone;
    bool latchHandMoveDone;

    bool configured;
    bool closed;
    bool checkEnabled;
    bool tracking_mode;
    bool torsoActive;
    bool verbose;

    double default_exec_time;
    double waitTmo;
    double latchTimer;
    double t0;

    int jHandMin;
    int jHandMax;

    yarp::sig::Vector      enableTorsoSw;
    yarp::sig::Vector      disableTorsoSw;

    yarp::sig::Vector      curGraspDetectionThres;
    std::set<int>          fingersJntsSet;
    std::set<int>          fingersMovingJntsSet;
    std::multimap<int,int> fingers2JntsMap;

    struct HandWayPoint
    {
        std::string       tag;
        yarp::sig::Vector poss;
        yarp::sig::Vector vels;
        yarp::sig::Vector thres;
    };

    struct Action
    {
        // wait action
        bool waitState;
        double tmo;
        // reach action
        bool execArm;
        yarp::sig::Vector x;
        yarp::sig::Vector o;
        double execTime;
        // hand action
        bool execHand;
        HandWayPoint handWP;
    };

    std::deque<Action> actionsQueue;
    std::map<std::string,std::deque<HandWayPoint> > handSeqMap;

    virtual std::string toCompactString(const yarp::sig::Vector &v);
    virtual int  printMessage(const char *format, ...);
    virtual bool handleTorsoDOF(yarp::os::Property &opt, const std::string &key,
                                const int j);
    virtual bool configHandSeq(yarp::os::Property &opt);
    virtual bool pushAction(const bool execArm, const yarp::sig::Vector &x,
                            const yarp::sig::Vector &o, const double execTime,
                            const bool execHand, const HandWayPoint &handWP);
    virtual bool stopJntTraj(const int jnt);
    virtual void enableTorsoDof();
    virtual void disableTorsoDof();
    virtual bool wait(const Action &action);
    virtual bool cmdArm(const Action &action);
    virtual bool cmdHand(const Action &action);
    virtual bool isHandSeqEnded();

    void init();    
    bool execQueuedAction();
    bool execPendingHandSequences();
    virtual void run();    

public:
    /**
    * Default Constructor. 
    */
    affActionPrimitives();

    /**
    * Constructor. 
    * @param opt the Property used to configure the object after its
    *            creation.
    */
    affActionPrimitives(yarp::os::Property &opt);

    /**
    * Destructor. 
    *  
    * \note it calls the close() method. 
    */
    virtual ~affActionPrimitives();

    /**
    * Configure the object.
    * @param opt the Property used to configure the object after its
    *            creation.
    *  
    * \note To be called after object creation. 
    *  
    * \note Available options are: 
    *  
    * \b local <string>: specify a stem name used to open local 
    *    ports and to highlight messages printed on the screen. 
    *  
    * \b robot <string>: the robot name to connect to (e.g. icub). 
    *  
    * \b part <string>:  the arm to be controlled (e.g. left_arm). 
    *  
    * \b thread_period <int>: the thread period [ms] which selects 
    *    the time granularity as well.
    *  
    * \b default_exec_time <double>: the arm movement execution time
    *    [s].
    *  
    * \b reach_tol <double>: the reaching tolerance [m]. 
    *  
    * \b tracking_mode <string>: enable/disable the tracking mode; 
    *    possible values: "on"/"off".
    * \note In tracking mode the cartesian position is mantained on 
    *       the reached target; in non-tracking mode the joints
    *       positions are kept once the target is attained.
    *  
    * \b verbosity <string>: enable/disable the verbose mode; 
    *    possible values: "on"/"off".
    *
    * \b torso_pitch <string>: if "on" it enables the control of the 
    *    pitch of the torso.
    *  
    * \b torso_pitch_min <double>: set the pitch minimum value 
    *    [deg].
    *  
    * \b torso_pitch_max <double>: set the pitch maximum value 
    *    [deg].
    *  
    * \b torso_roll <string>: if "on" it enables the control of the 
    *    roll of the torso.
    *  
    * \b torso_roll_min <double>: set the roll minimum value [deg]. 
    *  
    * \b torso_roll_max <double>: set the roll maximum value [deg].
    *  
    * \b torso_yaw <string>: if "on" it enables the control of the 
    *    yaw of the torso.
    *  
    * \b torso_yaw_min <double>: set the yaw minimum value [deg]. 
    *  
    * \b torso_yaw_max <double>: set the yaw maximum value [deg]. 
    *  
    * \b hand_sequences_file <string>: complete path to the file 
    *    containing the hand motions sequences.<br />Here is the
    *    format of motion sequences:
    *  
    *  \code
    *  [GENERAL]
    *  numSequences ***
    *  
    *  [SEQ_0]
    *  key ***
    *  numWayPoints ***
    *  wp_0  (poss (10 ...)) (vels (20 ...)) (thres (1 2 3 4 5))
    *  wp_1  ***
    *  ...
    *  
    *  [SEQ_1]
    *  ...
    *  
    *  // the "poss" and "vels" keys specify 9 joints positions and
    *  // velocities whereas the "thres" key specifies 5 fingers
    *  // thresholds used for model-based contact detection
    *  \endcode
    *  
    * \note A port called <i> /<local>/<part>/detectGrasp:i </i> is 
    *       open to acquire data provided by \ref icub_graspDetector
    *       module.
    */
    virtual bool open(yarp::os::Property &opt);

    /**
    * Check if the object is initialized correctly. 
    * @return true/fail on success/fail. 
    */
    virtual bool isValid();

    /**
    * Deallocate the object.
    */
    virtual void close();

    /**
    * Insert a combination of arm and hand primitive actions in the 
    * actions queue. 
    * @param x the 3-d target position [m]. 
    * @param o the 4-d hand orientation used while reaching (given 
    *          in axis-angle representation) [rad].
    * @param handSeqKey the hand sequence key. 
    * @param execTime the arm action execution time [s] (to be 
    *          specified iff different from default value).
    * @return true/false on success/fail. 
    *  
    * \note Some examples: 
    *  
    * the call \b pushAction(x,o,"close_hand") pushes the combined 
    * action of reach(x,o) and hand "close_hand" sequence into the 
    * queue; the action will be executed as soon as all the previous 
    * items in the queue will have been served. 
    */
    virtual bool pushAction(const yarp::sig::Vector &x, const yarp::sig::Vector &o, 
                            const std::string &handSeqKey,
                            const double execTime=ACTIONPRIM_DISABLE_EXECTIME);

    /**
    * Insert the arm-primitive action reach for target in the 
    * actions queue. 
    * @param x the 3-d target position [m]. 
    * @param o the 4-d hand orientation used while reaching (given 
    *          in axis-angle representation) [rad].
    * @param execTime the arm action execution time [s] (to be 
    *          specified iff different from default value).
    * @return true/false on success/fail. 
    */
    virtual bool pushAction(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                            const double execTime=ACTIONPRIM_DISABLE_EXECTIME);

    /**
    * Insert a hand-primitive action in the actions queue.
    * @param handSeqKey the hand sequence key.   
    * @return true/false on success/fail. 
    */
    virtual bool pushAction(const std::string &handSeqKey);

    /**
    * Insert a wait state in the actions queue.
    * @param tmo is the wait timeout [s]. 
    * @return true/false on success/fail. 
    */
    virtual bool pushWaitState(const double tmo);

    /**
    * Immediately update the current reaching target (without 
    * affecting the actions queue) or initiate a new reach (if the 
    * actions queue is empty).
    * @param x the 3-d target position [m]. 
    * @param o the 4-d hand orientation used while reaching (given 
    *          in axis-angle representation) [rad].
    * @param execTime the arm action execution time [s] (to be 
    *          specified iff different from default value).
    * @return true/false on success/fail. 
    *  
    * \note The intended use is for tracking moving targets. 
    */
    virtual bool reach(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                       const double execTime=ACTIONPRIM_DISABLE_EXECTIME);

    /**
    * Empty the actions queue. 
    * @return true/false on success/fail.
    */
    virtual bool clearActionsQueue();

    /**
    * Define an hand WayPoint (WP) to be added at the bottom of the 
    * hand motion sequence pointed by the key. 
    * @param handSeqKey the hand sequence key.
    * @param poss the 9 fingers joints WP positions to be attained 
    *             [deg].
    * @param vels the 9 fingers joints velocities [deg/s]. 
    * @param thres the 5 fingers thresholds used for grasp 
    *              detection.
    * @return true/false on success/fail. 
    *  
    * \note this method creates a new empty sequence referred by the
    *       passed key if the key does not point to any valid
    *       sequence; hence the triplet (poss,vels,thres) will be
    *       the first WP of the new sequence.
    */
    virtual bool addHandSeqWP(const std::string &handSeqKey, const yarp::sig::Vector &poss,
                              const yarp::sig::Vector &vels, const yarp::sig::Vector &thres);

    /**
    * Check whether a sequence key is defined.
    * @param handSeqKey the hand sequence key.
    * @return true iff valid key. 
    */
    virtual bool isValidHandSeq(const std::string &handSeqKey);

    /**
    * Remove an already existing hand motion sequence.
    * @param handSeqKey the hand sequence key.
    * @return true/false on success/fail. 
    */
    virtual bool removeHandSeq(const std::string &handSeqKey);

    /**
    * Return the whole list of available hand sequence keys.
    * @return the list of available hand sequence keys.
    */
    std::deque<std::string> getHandSeqList();

    /**
    * Return the current end-effector position. 
    * @param x the current 3-d hand position [m].
    * @param o the current 4-d hand orientation (given in the 
    *          axis-angle representation) [rad].
    * @return true/false on success/fail.
    */
    virtual bool getPose(yarp::sig::Vector &x, yarp::sig::Vector &o);

    /**
    * Stop any ongoing arm/hand movements.
    * @return true/false on success/fail. 
    *  
    * \note it empty out the actions queue. 
    */
    virtual bool stopControl();

    /**
    * Set the task space controller in tracking or non-tracking 
    * mode. 
    * @param f: true for tracking mode, false otherwise. 
    * \note In tracking mode the cartesian position is mantained on 
    *       the reached target; in non-tracking mode the joints
    *       positions are kept once the target is attained.
    * @return true/false on success/failure.
    */
    virtual bool setTrackingMode(const bool f);

    /**
    * Get the current controller mode.
    * @return true/false on tracking/non-tracking mode. 
    */
    virtual bool getTrackingMode();

    /**
    * Check whether the action is accomplished or still ongoing.
    * @param f the result of the check. 
    * @param sync if true wait until the action is accomplished 
    *             (blocking call).
    * @return true/false on success/fail. 
    *  
    * \note Actually the check is performed on the content of the 
    *       actions queue so that the blocking call returns as soon
    *       as the queue is empty.
    */
    virtual bool checkActionsDone(bool &f, const bool sync=false);

    /**
    * Suddenly interrupt any blocking call that is pending on 
    * querying the action status. 
    * @param disable disable the blocking feature for future 
    * calls with sync switch on; useful to allow a graceful stop of 
    * the application. \see syncCheckReinstate to reinstate it.
    * @return true/false on success/fail. 
    */
    virtual bool syncCheckInterrupt(const bool disable=false);

    /**
    * Reinstate the blocking feature for future calls with sync 
    * switch on. 
    * @return true/false on success/fail. 
    */
    virtual bool syncCheckReinstate();
};


/**
* \ingroup affActionPrimitives
*
* A derived class defining a first abstraction layer on top of 
* affActionPrimitives father class. 
*  
* It internally predeclares (without actually defining) a set of
* hand sequence motions key ("open_hand", "close_hand" and 
* "karate_hand" :) that are used for grasp(), touch() and tap() 
* actions. 
*  
* \note Given as an example of how primitive actions can be 
*       combined in higher level actions, thus how further
*       layers can be inherited from the base class.
*/
class affActionPrimitivesLayer1 : public affActionPrimitives
{
public:
    /**
    * Default Constructor. 
    */
    affActionPrimitivesLayer1() : affActionPrimitives() { }

    /**
    * Constructor. 
    * @param opt the Property used to configure the object after its
    *            creation.
    */
    affActionPrimitivesLayer1(yarp::os::Property &opt) : affActionPrimitives(opt) { }

    /**
    * Grasp the given target (combined action).
    * @param x the 3-d target position [m]. 
    * @param o the 4-d hand orientation used while reaching/grasping
    *          (given in axis-angle representation) [rad].
    * @param d the displacement [m] wrt the target position that 
    *          identifies a location to be reached prior to
    *          grasping.
    * @return true/false on success/fail. 
    *  
    * \note internal implementation: 
    * \code 
    * ... 
    * pushAction(x+d,o,"open_hand"); 
    * pushAction(x,o); 
    * pushAction("close_hand") 
    * ... 
    * \endcode 
    *  
    * It reachs for (x+d,o) opening the hand, then reachs for (x,o)
    * and finally closes the hand. 
    */
    virtual bool grasp(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                       const yarp::sig::Vector &d);

    /**
    * Touch the given target (combined action).
    * @param x the 3-d target position [m]. 
    * @param o the 4-d hand orientation used while reaching/touching
    *          (given in axis-angle representation) [rad].
    * @param d the displacement [m] wrt the target position that 
    *          identifies a location to be reached prior to
    *          touching.
    * @return true/false on success/fail. 
    *  
    * \note internal implementation: 
    * \code 
    * ... 
    * pushAction(x+d,o,"open_hand"); 
    * pushAction(x,o); 
    * ... 
    * \endcode 
    *  
    * It reachs for (x+d,o), then reachs for (x,o). 
    * Similar to grasp but without final hand action. 
    */
    virtual bool touch(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                       const yarp::sig::Vector &d);

    /**
    * Tap the given target (combined action).
    * @param x1 the fisrt 3-d target position [m]. 
    * @param o1 the first 4-d hand orientation (given in axis-angle 
    *           representation) [rad].
    * @param x2 the second 3-d target position [m]. 
    * @param o2 the second 4-d hand orientation (given in axis-angle
    *           representation) [rad].
    * @param execTime the arm action execution time only while 
    *          tapping [s] (to be specified iff different from
    *          default value).
    * @return true/false on success/fail. 
    *  
    * \note internal implementation: 
    * \code 
    * ...
    * pushAction(x1,o1,"karate_hand");
    * pushAction(x2,o2,execTime);
    * pushAction(x1,o1); 
    * ... 
    * \endcode 
    *  
    * It reachs for (x1,o1), then reachs for (x2,o2) and then again
    * for (x1,o1).
    */
    virtual bool tap(const yarp::sig::Vector &x1, const yarp::sig::Vector &o1,
                     const yarp::sig::Vector &x2, const yarp::sig::Vector &o2,
                     const double execTime=ACTIONPRIM_DISABLE_EXECTIME);
};


/**
* \ingroup affActionPrimitives
*
* A class that inherits from affActionPrimitivesLayer1 modifying 
* the grasp() and touch() primitives in the following way: 
*  
* While reaching for the object, one wrist joint is kept fixed 
* (exploting however the torso dof the orientation of the hand 
* can be still fully controlled) in order to detect contact by 
* thresholding the low-level output signal. As soon as the 
* contact is detected the reaching is suddenly stopped. 
*  
* \note Unlike the previous implementation of grasp() and 
* touch(), the height of the object can be known just 
* approximately. 
*/
class affActionPrimitivesLayer2 : public affActionPrimitivesLayer1
{
public:
    affActionPrimitivesLayer2() : affActionPrimitivesLayer1() { }
    affActionPrimitivesLayer2(yarp::os::Property &opt) : affActionPrimitivesLayer1(opt) { }

    /**
    * Configure the object.
    * @param opt the Property used to configure the object after its
    *            creation.
    *  
    * \note To be called after object creation. 
    *  
    * \note Further available options are: 
    *  
    * \b wrist_joint (<int> <double>): specify the wrist joint to be 
    *    blocked while grasping/touching and the corresponding
    *    threshold for the output signal to detect contact with
    *    object.
    * 
    */
    virtual bool open(yarp::os::Property &opt);

    virtual bool grasp(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                       const yarp::sig::Vector &d);
    virtual bool touch(const yarp::sig::Vector &x, const yarp::sig::Vector &o,
                       const yarp::sig::Vector &d);
};


#endif


