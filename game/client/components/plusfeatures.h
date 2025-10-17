#pragma once
#include <game/client/component.h>
#include <game/client/prediction/entities/character.h>	
#include <game/generated/protocol.h>
#include <game/client/prediction/gameworld.h>
#include <base/vmath.h>
#include <engine/graphics.h>

#define IDLE_TIMER 1.f;
#define ANGRY_TIMER .3f;

typedef struct MovementRecord
	{
		std::vector<vec2> recordsPositions;
		std::vector<CNetObj_PlayerInput> recordsActions;
		std::vector<vec2> recordsMouse;
		std::vector<int> recordsDummySwitch;
		std::vector<int> recordsKill;
	} MovementRecord;

class CPlusFeatures : public CComponent {
    public:
        bool NeedReset = false;

        void AutoAim();
        void FlyBot();
        void AntiFallingFreeze();
        void AntiGoingFreeze(int type);
        void PredictionBot(int predictTicks, CGameWorld *curWorld);

        vec2 normalize(vec2 v);
        void AutoWallshot();
        float lerp(float a, float b, float t);

        void SetIdle();
        void SetFollow();
        void SetAngry();

        void RenderPetPositionLine();
        vec2 PlayerPathNormalization(vec2 player);
        void Pet();
        void RenderPet();
        void GoTo(vec2 path);
        void PetStateUpdate();

        void RenderTrail(float rSize, int color, vec2 pos);
        void Trail(vec2 pos, float timePassed, ColorRGBA c);
        void MagicParticles(float radius);
	    void MagicParticles2(float radius, vec2 pos);

	    struct TestResult
	    {
		    int lifetime = -1;
		    CNetObj_PlayerInput input = {};
	    };

        std::vector<const char *> GetBinaryFilesInFolder(const std::string &folderPath);
        void SaveRecordsToFile(const std::string &filename);
        void LoadRecordsFromFile(const std::string &filename);
        void SaveRecord();
        void DeleteRecord(const std::string &filename);
        void RenderPath();
        void DrawSnapline();
        void DrawBox();
        void DrawPredictedPos();
	    void RenderPositionLine(vec2 target1, vec2 target2);
        void RenderStart();
        void Record();
        void Play();
        void Spinbot();
	    void Stabilizer();
	    void HammerCopy();
        void Aim(vec2 dir);
	    int CheckPredictions(CNetObj_PlayerInput *inputData, int predictTicks, CGameWorld *curWorld);
	    CNetObj_PlayerInput FindBestInput(const std::vector<TestResult> &tests);
	    CNetObj_PlayerInput GetAction(int ticks, std::vector<TestResult> *results);
        std::vector<int> m_ResultActions;
	    CCharacter *PredictTicks(int predictTicks, CNetObj_PlayerInput *inputData, int id);

        MovementRecord records;

        struct WallshotLine
        {
            vec2 start;
            vec2 end;
        };

        virtual int Sizeof() const override { return sizeof(*this); }
        virtual void OnRender() override;        
    private:
        int GetNearestPlayer(float maxDist);    
        void HandleUnGrabFastHook();
	    bool CanHook(int predTicks, const CNetObj_PlayerInput *inputState, CGameWorld *baseWorld);
        void GetLaserWallshot(std::vector<WallshotLine>& outLines, vec2 StartPos, vec2 TargetPos, float circleRadius, int bounce);

        void GetLaserLines(std::vector<WallshotLine>* lines, vec2 start, vec2 dir, float reach, int bounce, int layer);
        bool GetLineCircleIntersection(vec2 center, float radius,
            const WallshotLine *line,
            vec2 *intersection1, vec2 *intersection2);
        bool CheckPointIsOnLine(const WallshotLine *line, vec2 point);
        vec2 NormalizeAim(vec2 Pos);

        enum PetState
        {
            IDLE,
            FOLLOW,
            ANGRY,

        };

        float timerValue = 0.2f;
        float timer = timerValue;

        vec2 PetPos;
        vec2 PetPosOld;
        vec2 PetVel;
        float speed = 15000;
        float acceleration = 100;
        PetState PetState = FOLLOW;

        vec2 m_PetTarget;

        float maxDistance = 250;
        float minDistance = 70;

        float StateTimer = IDLE_TIMER;
        vec2 target;

        float timerIdle = 0;

        bool petMoving = false;

        static char *strdup_0(const char *str)
        {
            if(!str)
                return nullptr;
            size_t len = strlen(str);
            char *copy = new char[len + 1];
            memcpy(copy, str, len);
            copy[len] = '\0';
            return copy;
        }
};