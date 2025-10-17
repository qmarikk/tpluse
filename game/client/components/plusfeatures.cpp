#include "plusfeatures.h"

#include <base/vmath.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <base/system.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include "base/math.h"
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

vec2 CPlusFeatures::normalize(vec2 v)
{
	float length = sqrt(v.x * v.x + v.y * v.y);
	return vec2(v.x / length, v.y / length);
}

float CPlusFeatures::lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

vec2 CPlusFeatures::PlayerPathNormalization(vec2 player)
{
	vec2 d = vec2(player.x - PetPos.x, player.y - PetPos.y);
	return vec2(normalize(d).x, normalize(d).y);
}

void CPlusFeatures::GoTo(vec2 path)
{
	float distanceFactor = 1.0f - (distance(m_PetTarget, PetPos) / maxDistance);
	float distanceAcceleration = acceleration * distanceFactor;

	speed += distanceAcceleration * Client()->RenderFrameTime();

	PetPos.x += path.x * speed * Client()->RenderFrameTime();
	PetPos.y += path.y * speed * Client()->RenderFrameTime();
}

void CPlusFeatures::SetIdle()
{
	PetState = IDLE;
	timerIdle += Client()->RenderFrameTime() * 50;
	vec2 newTarget = PlayerPathNormalization(vec2(PetPos.x, PetPos.y + sin(timerIdle))); 
	target.x = lerp(target.x, newTarget.x, 0.1f); 
	target.y = lerp(target.y, newTarget.y, 0.1f); 
	speed = 15 + sin(timerIdle);
}

void CPlusFeatures::SetFollow()
{
	PetState = FOLLOW;
	target = PlayerPathNormalization(m_PetTarget);
	speed = 400;
}

void CPlusFeatures::SetAngry()
{
	PetState = ANGRY;
	StateTimer = ANGRY_TIMER;
	speed = 0;
}

void CPlusFeatures::PetStateUpdate()
{
	switch(PetState)
	{
	case IDLE:
		StateTimer -= Client()->RenderFrameTime();
		if(StateTimer <= 0 && g_Config.m_ClPlusRenderPetLikeTee)
		{
			SetAngry();
		}
		else
		{
			SetIdle();
		}
		petMoving = false;
		break;
	case ANGRY:
		StateTimer -= Client()->RenderFrameTime();
		if(StateTimer <= 0)
		{
			StateTimer = IDLE_TIMER;
			SetIdle();
		}
		petMoving = false;
		break;

	case FOLLOW:
		if(distance(m_PetTarget, PetPos) <= minDistance)
		{
			StateTimer = IDLE_TIMER;
			SetIdle();
		}
		else
		{
			target = PlayerPathNormalization(m_PetTarget);
		}
		petMoving = true;
		break;

	default:
		break;
	}

	if(distance(m_PetTarget, PetPos) >= maxDistance || (PetPos.x < 0 || PetPos.y < 0))
		SetFollow();

}

void CPlusFeatures::OnRender()
{
	if(g_Config.m_ClPlusPet)
	{
		m_PetTarget = m_pClient->m_aClients[m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget]].m_Predicted.m_Pos;
		Pet();

		if(g_Config.m_ClPlusPetPositionLine)
		{
			vec2 target2{m_PetTarget.x, m_PetTarget.y};
			vec2 target1{PetPos.x, PetPos.y};
			RenderPositionLine(target1, target2);
		}

		if(g_Config.m_ClPlusPetTrail && petMoving)
		{
			RenderTrail(g_Config.m_ClPlusPetTrailRadius, g_Config.m_ClPlusPetTrailColor, PetPos);
		}
	}

	if(g_Config.m_ClPlusTeeTrail)
	{
		const auto &vel = m_pClient->m_PredictedChar.m_Vel;
		vec2 zeroVec{0.0f, 0.0f};

		if(vel != zeroVec)
		{
			RenderTrail(g_Config.m_ClPlusTeeTrailRadius,
				g_Config.m_ClPlusTeeTrailColor,
				m_pClient->m_LocalCharacterPos);
		}
	}

	if(g_Config.m_ClPlusMagicParticles)
	{
		MagicParticles(50.0f);
	}

	if(g_Config.m_ClPlusMagicParticlesVariant)
	{
		MagicParticles2(80.0f, m_pClient->m_LocalCharacterPos);
	}

	if(!records.recordsPositions.empty())
	{
		if(g_Config.m_ClPlusRenderBluePathLine && g_Config.m_ClPlusPlaying)
		{
			RenderPath();
		}
		else if(!g_Config.m_ClPlusPlaying && !g_Config.m_ClPlusRecording)
		{
			RenderStart();
		}
	}

	if(g_Config.m_ClPlusSnapline)
	{
		DrawSnapline();
	}

	if(g_Config.m_ClPlusBox)
	{
		DrawBox();
	}

	if(g_Config.m_ClPlusDrawPredictedPos)
	{
		DrawPredictedPos();
	}
}

void CPlusFeatures::DrawPredictedPos()
{
	auto *graphics = Graphics();
	graphics->TextureClear();

	std::vector<vec2> positions;
	auto *curWorld = &m_pClient->m_PredictedWorld;
	CGameWorld tempWorld(*curWorld);

	auto *pTempChar = tempWorld.GetCharacterByID(m_pClient->m_aLocalIDs[g_Config.m_ClDummy]);
	if(!pTempChar)
		return;

	CNetObj_PlayerInput tempInputData = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];

	for(int tick = 0; tick <= 20; ++tick)
	{
		++tempWorld.m_GameTick;
		pTempChar->OnPredictedInput(&tempInputData);
		tempWorld.Tick();
		positions.push_back(pTempChar->m_Pos);
	}

	if(positions.empty())
		return;

	float lineWidth = 1.25f;
	for(size_t i = 0; i < positions.size() - 1; ++i)
	{
		vec2 direction = normalize(vec2(
			positions[i + 1].x - positions[i].x,
			positions[i + 1].y - positions[i].y));

		vec2 perpToAngle(-direction.y, direction.x);
		float zoom = m_pClient->m_Camera.m_Zoom;
		perpToAngle = vec2(perpToAngle.x * lineWidth * zoom, perpToAngle.y * lineWidth * zoom);

		vec2 pos0 = positions[i] + perpToAngle;
		vec2 pos1 = positions[i] - perpToAngle;
		vec2 pos2 = positions[i + 1] + perpToAngle;
		vec2 pos3 = positions[i + 1] - perpToAngle;

		graphics->QuadsBegin();
		graphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		IGraphics::CFreeformItem item(
			pos0.x, pos0.y,
			pos1.x, pos1.y,
			pos2.x, pos2.y,
			pos3.x, pos3.y);

		graphics->QuadsDrawFreeform(&item, 1);
		graphics->QuadsEnd();
	}
}

void CPlusFeatures::DrawBox()
{
	auto *graphics = Graphics();
	graphics->TextureClear();

	ColorRGBA color(0, 0, 0, 1);
	int nearestPlayer = GetNearestPlayer(9999.0f);

	auto &teams = m_pClient->m_Teams;
	if(teams.Team(m_pClient->m_Snap.m_LocalClientID) != 0)
		return;

	for(int i = 0; i < 64; ++i)
	{
		if(!m_pClient->m_Snap.m_apPlayerInfos[i] || i == m_pClient->m_Snap.m_LocalClientID)
			continue;

		const auto &clientData = m_pClient->m_aClients[i];

		if(clientData.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team &&
			!clientData.m_Spec &&
			m_pClient->m_Snap.m_aCharacters[i].m_Active &&
			!clientData.m_FreezeEnd &&
			!clientData.m_DeepFrozen)
		{
			graphics->LinesBegin();

			if(clientData.m_Predicted.m_Id == nearestPlayer)
				graphics->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			else
				graphics->SetColor(0.0f, 1.0f, 0.0f, 1.0f);

			float x = clientData.m_RenderPos.x;
			float y = clientData.m_RenderPos.y;
			float size = 32.0f;

			IGraphics::CLineItem lines[] = {
				{x - size, y - size, x + size, y - size},
				{x - size, y + size, x + size, y + size},
				{x - size, y - size, x - size, y + size},
				{x + size, y - size, x + size, y + size}};

			graphics->LinesDraw(lines, 4);
			graphics->LinesEnd();
		}
	}
}

void CPlusFeatures::RenderPositionLine(vec2 target1, vec2 target2)
{
	auto *graphics = Graphics();
	graphics->TextureClear();

	auto *renderTools = RenderTools();
	float zoom = m_pClient->m_Camera.m_Zoom;
	float centerY = m_pClient->m_Camera.m_Center.y;
	float centerX = m_pClient->m_Camera.m_Center.x;
	renderTools->MapScreenToInterface(centerX, centerY);

	graphics->LinesBegin();
	graphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	IGraphics::CLineItem line(
		target1.x, target1.y,
		target2.x, target2.y);

	graphics->LinesDraw(&line, 1);
	graphics->LinesEnd();
}

void CPlusFeatures::Spinbot()
{
	if(g_Config.m_ClPlusSpinbotAutoFire)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire++;
	}

	float time = LocalTime();
	float angle = time * static_cast<float>(g_Config.m_ClPlusSpinbotSpeed);
	float distance = static_cast<float>(g_Config.m_ClPlusSpinbotDistance);

	if(g_Config.m_ClPlusSpinbotSilent)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetX =
			static_cast<int>(std::cos(angle) * distance);
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetY =
			static_cast<int>(std::sin(angle) * distance);
	}
	else
	{
		m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x = std::cos(angle) * distance;
		m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y = std::sin(angle) * distance;
	}
}

void CPlusFeatures::Stabilizer()
{
	int targetId = GetNearestPlayer(static_cast<float>(g_Config.m_ClPlusAutoAimRange));
	if(targetId < 0)
	{
		return;
	}

	const auto &localPos = m_pClient->m_LocalCharacterPos;
	const auto &targetPos = m_pClient->m_aClients[targetId].m_RenderPos;

	if(localPos.x > targetPos.x)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = -1;
	}
	if(targetPos.x > localPos.x)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = 1;
	}
}

void CPlusFeatures::DrawSnapline()
{
	auto *graphics = Graphics();
	graphics->TextureClear();

	ColorRGBA color(0, 0, 0, 1);
	int nearestPlayer = GetNearestPlayer(9999.0f);

	auto &teams = m_pClient->m_Teams;
	if(teams.Team(m_pClient->m_Snap.m_LocalClientID) != 0)
		return;

	for(int i = 0; i < 64; ++i)
	{
		if(!m_pClient->m_Snap.m_apPlayerInfos[i] || i == m_pClient->m_Snap.m_LocalClientID)
			continue;

		const auto &clientData = m_pClient->m_aClients[i];

		if(clientData.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team &&
			!clientData.m_Spec &&
			m_pClient->m_Snap.m_aCharacters[i].m_Active &&
			!clientData.m_FreezeEnd &&
			!clientData.m_DeepFrozen)
		{
			graphics->LinesBegin();

			if(clientData.m_Predicted.m_Id == nearestPlayer)
				graphics->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			else
				graphics->SetColor(0.0f, 1.0f, 0.0f, 1.0f);

			IGraphics::CLineItem line(
				m_pClient->m_Camera.m_Center.x,
				m_pClient->m_Camera.m_Center.y,
				clientData.m_RenderPos.x,
				clientData.m_RenderPos.y);

			graphics->LinesDraw(&line, 1);
			graphics->LinesEnd();
		}
	}
}

void CPlusFeatures::RenderStart()
{
	auto *graphics = Graphics();
	graphics->TextureClear();

	auto *renderTools = RenderTools();
	float zoom = m_pClient->m_Camera.m_Zoom;
	float centerY = m_pClient->m_Camera.m_Center.y;
	float centerX = m_pClient->m_Camera.m_Center.x;

	renderTools->MapScreenToInterface(centerX, centerY);

	CTeeRenderInfo pInfo = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_RenderInfo;

	vec2 pos = records.recordsPositions[0];
	vec2 dir(1.0f, 0.4f);

	const CAnimState *pAnim = CAnimState::GetIdle();
	renderTools->RenderTee(pAnim, &pInfo, 3, dir, pos, 0.2f);

	RenderPositionLine(m_pClient->m_LocalCharacterPos, records.recordsPositions[0]);
}

void CPlusFeatures::RenderPath()
{
	Graphics()->TextureClear();
	RenderTools()->MapScreenToInterface(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y);

	for(int i = 0; i < (int)records.recordsPositions.size() - 1; i++)
	{
		Graphics()->LinesBegin();
		Graphics()->SetColor(0.18, 0.56, 0.9, .3f);
		IGraphics::CLineItem Line(records.recordsPositions[i].x, records.recordsPositions[i].y, records.recordsPositions[i + 1].x, records.recordsPositions[i + 1].y);
		Graphics()->LinesDraw(&Line, 1);
		Graphics()->LinesEnd();

		if(i % 10 == 0)
		{
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.22, 0.10, 0.9, 1);
			Graphics()->DrawCircle(records.recordsPositions[i].x, records.recordsPositions[i].y, 3.0f, 64);
			Graphics()->QuadsEnd();
		}
	}
}

void CPlusFeatures::Pet()
{
	if(m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget] == -1 || !m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget]].m_Active)
		return; 

	if(distance(m_PetTarget, PetPos) > 1000)
		speed = 15000;

	GoTo(target);

	RenderPet();
}

void CPlusFeatures::RenderPet()
{
	if(m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget] == -1 || !m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget]].m_Active)
		return; 

	Graphics()->TextureClear();
	RenderTools()->MapScreenToInterface(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y);

	if(g_Config.m_ClPlusRenderPetLikeTee)
	{
		CTeeRenderInfo pInfo = m_pClient->m_aClients[m_pClient->m_aLocalIDs[g_Config.m_ClPlusPetTarget]].m_RenderInfo;

		RenderTools()->RenderTee(CAnimState::GetIdle(), &pInfo, (PetState == ANGRY ? EMOTE_ANGRY : EMOTE_NORMAL), vec2(1, 0.4f), PetPos, 1);
	}
	else
	{
		int petWidth = 35;
		int petHeight = 45;

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PET].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1, 1, 1, 1);
		IGraphics::CQuadItem QuadItem = IGraphics::CQuadItem(PetPos.x - petWidth / 2, PetPos.y - petHeight / 2, petWidth, petHeight);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	PetVel = vec2((PetPos.x - PetPosOld.x) / Client()->RenderFrameTime(), (PetPos.y - PetPosOld.y) / Client()->RenderFrameTime());

	PetPosOld = PetPos;
}

void CPlusFeatures::RenderPetPositionLine()
{
	Graphics()->TextureClear();
	RenderTools()->MapScreenToInterface(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y);

	Graphics()->LinesBegin();
	Graphics()->SetColor(.5, .7, 0, 1);

	IGraphics::CLineItem Line(PetPos.x, PetPos.y, m_PetTarget.x, m_PetTarget.y);
	Graphics()->LinesDraw(&Line, 1);
	Graphics()->LinesEnd();
}

void CPlusFeatures::RenderTrail(float rSize, int color, vec2 pos)
{
	ColorRGBA c = color_cast<ColorRGBA>(ColorHSLA(color));

	int r = random_float(-rSize / 10, rSize / 10);

	Trail({pos.x + r, pos.y + r}, Client()->RenderFrameTime(), c);
}

void CPlusFeatures::Trail(vec2 pos, float timePassed, ColorRGBA c)
{
	if(timePassed < 0.001f)
		return;

	for(int i = 0; i < 5; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_BALL;
		p.m_Pos = pos;
		p.m_LifeSpan = random_float(0.25f, 0.5f);
		p.m_StartSize = 8.0f;
		p.m_EndSize = 0;
		p.m_Friction = 0.7f;
		p.m_Color = c;
		p.m_StartAlpha = c.a;
		m_pClient->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, timePassed);
	}
}

void CPlusFeatures::MagicParticles(float radius)
{
	for(int i = 0; i < 10; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_BALL;

		float angle = random_float(0, 2 * pi);
		float dist = random_float(0, radius);
		vec2 offset = vec2(cos(angle) * dist, sin(angle) * dist);

		p.m_Pos = m_pClient->m_LocalCharacterPos + offset;

		p.m_LifeSpan = random_float(0.25f, 0.5f);
		p.m_StartSize = 8.0f;
		p.m_EndSize = 0;
		p.m_Friction = 0.7f;
		p.m_Color = ColorRGBA(random_float(0, 1.0f), random_float(0, 1.0f), random_float(0, 1.0f), random_float(0.5f, 1.0f));
		p.m_StartAlpha = p.m_Color.a;
		m_pClient->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, Client()->RenderFrameTime());
	}
}

void CPlusFeatures::MagicParticles2(float radius, vec2 pos)
{
	timer -= Client()->RenderFrameTime();
	if(timer <= 0)
	{
		timer = timerValue;
		for(int i = 0; i < 30; i++)
		{
			CParticle p;
			p.SetDefault();
			p.m_Spr = SPRITE_PART_BALL;

			float angle = random_float(0, 2 * pi);
			float dist = random_float(0, radius);
			vec2 offset = vec2(cos(angle) * dist, sin(angle) * dist);

			p.m_Pos = pos + offset;

			p.m_LifeSpan = random_float(0.5f, 1.5f);
			p.m_StartSize = 8.0f;
			p.m_EndSize = 0;
			p.m_Friction = 0.7f;
			p.m_Color = ColorRGBA(random_float(0, 1.0f), random_float(0, 1.0f), random_float(0, 1.0f), random_float(0.2f, 1.0f));
			p.m_StartAlpha = p.m_Color.a;
			m_pClient->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, Client()->RenderFrameTime());
		}
	}
}

std::vector<const char *> CPlusFeatures::GetBinaryFilesInFolder(const std::string &folderPath)
{
	std::vector<const char *> fileNames;

	try
	{
		for(const auto &entry : std::filesystem::directory_iterator(folderPath))
		{
			if(!entry.is_regular_file())
			{
				continue;
			}

			auto ext = entry.path().extension();
			if(ext == ".bin" || ext == ".kr")
			{
				std::string fileName = entry.path().string();
				fileNames.push_back(strdup_0(fileName.c_str()));
			}
		}
	}
	catch(const std::filesystem::filesystem_error &e)
	{
		dbg_msg("TClientPlus", "Error accessing directory: %s", e.what());
	}

	return fileNames;
}

template<typename T>
void deserializeVector(std::ifstream &ifs, std::vector<T> *vec)
{
	size_t size;
	ifs.read(reinterpret_cast<char *>(&size), sizeof(size));
	vec->resize(size);
	ifs.read(reinterpret_cast<char *>(vec->data()), size * sizeof(T));
}

template<typename T>
void serializeVector(std::ofstream &ofs, const std::vector<T> *vec)
{
	size_t size = vec->size();
	ofs.write(reinterpret_cast<const char *>(&size), sizeof(size));
	ofs.write(reinterpret_cast<const char *>(vec->data()), size * sizeof(T));
}

void CPlusFeatures::SaveRecordsToFile(const std::string &filename)
{
	std::ofstream ofs(filename, std::ios::binary);
	if(!ofs.is_open())
	{
		return;
	}

	serializeVector<vec2>(ofs, &records.recordsPositions);
	serializeVector<CNetObj_PlayerInput>(ofs, &records.recordsActions);
	serializeVector<vec2>(ofs, &records.recordsMouse);
	serializeVector<int>(ofs, &records.recordsDummySwitch);
	serializeVector<int>(ofs, &records.recordsKill);

	ofs.close();
}

void CPlusFeatures::LoadRecordsFromFile(const std::string &filename)
{
	std::ifstream ifs(filename, std::ios::binary);
	if(!ifs.is_open())
	{
		return;
	}

	deserializeVector<vec2>(ifs, &records.recordsPositions);
	deserializeVector<CNetObj_PlayerInput>(ifs, &records.recordsActions);
	deserializeVector<vec2>(ifs, &records.recordsMouse);
	deserializeVector<int>(ifs, &records.recordsDummySwitch);
	deserializeVector<int>(ifs, &records.recordsKill);

	ifs.close();
}

void CPlusFeatures::SaveRecord()
{
	std::string mapName = Client()->GetCurrentMap();

	time_t now = time();
	tm *ltm = localtime(&now);

	char buffer[100];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S", ltm);

	std::string filePath = std::string(g_Config.m_ClPlusMapRecordsPath) +
			       "\\" + mapName + "_" + buffer + ".kr";

	SaveRecordsToFile(filePath);
}

void CPlusFeatures::DeleteRecord(const std::string &filename)
{
	if(std::remove(filename.c_str()) != 0)
	{
		dbg_msg("TClientPlus", "Error deleting the file: %s", filename.c_str());
	}
	else
	{
		dbg_msg("TClientPlus", "File successfully deleted: %s", filename.c_str());
	}
}

void CPlusFeatures::Record()
{
	records.recordsActions.push_back(m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy]);

	records.recordsMouse.push_back(m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy]);

	records.recordsPositions.push_back(m_pClient->m_LocalCharacterPos);

	records.recordsDummySwitch.push_back(g_Config.m_ClDummy);

	records.recordsKill.push_back(g_Config.m_ClPlusKilled);
	if(g_Config.m_ClPlusKilled)
	{
		g_Config.m_ClPlusKilled = 0;
	}
}

void CPlusFeatures::Play()
{
	if(records.recordsActions.empty())
	{
		return;
	}

	m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy] = records.recordsActions[0];

	m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] = records.recordsMouse[0];

	g_Config.m_ClDummy = records.recordsDummySwitch[0];

	if(records.recordsKill[0])
	{
		m_pClient->SendKill(-1);
	}

	records.recordsActions.erase(records.recordsActions.begin());
	records.recordsMouse.erase(records.recordsMouse.begin());
	records.recordsPositions.erase(records.recordsPositions.begin());
	records.recordsDummySwitch.erase(records.recordsDummySwitch.begin());
	records.recordsKill.erase(records.recordsKill.begin());
}

int CPlusFeatures::GetNearestPlayer(float maxDist)
{
	float nearestDist = maxDist;
	int nearestId = -1;

	auto &teams = m_pClient->m_Teams;
	if(teams.Team(m_pClient->m_Snap.m_LocalClientID) != 0)
	{
		return -1;
	}

	for(int i = 0; i < 64; ++i)
	{
		if(!m_pClient->m_Snap.m_apPlayerInfos[i] || i == m_pClient->m_Snap.m_LocalClientID)
		{
			continue;
		}

		const auto &clientData = m_pClient->m_aClients[i];

		if(clientData.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team &&
			!clientData.m_Spec &&
			m_pClient->m_Snap.m_aCharacters[i].m_Active &&
			!clientData.m_FreezeEnd &&
			!clientData.m_DeepFrozen)
		{
			float dist = distance(m_pClient->m_PredictedChar.m_Pos, clientData.m_Predicted.m_Pos);
			if(dist < nearestDist && dist < maxDist)
			{
				nearestDist = dist;
				nearestId = clientData.m_Predicted.m_Id;
			}
		}
	}

	return nearestId;
}

CCharacter *CPlusFeatures::PredictTicks(int predictTicks, CNetObj_PlayerInput *inputData, int id)
{
	if(!&m_pClient->m_GameWorld)
		return nullptr;

	CGameWorld predictWorld;
	predictWorld.m_pCollision = m_pClient->m_GameWorld.m_pCollision;
	predictWorld.m_GameTick = m_pClient->m_GameWorld.m_GameTick;
	predictWorld.m_GameTickSpeed = m_pClient->m_GameWorld.m_GameTickSpeed;
	predictWorld.m_WorldConfig = m_pClient->m_GameWorld.m_WorldConfig;
	predictWorld.m_Teams = m_pClient->m_GameWorld.m_Teams;
	predictWorld.CopyWorld(&m_pClient->m_GameWorld);

	CCharacter *character = predictWorld.GetCharacterByID(id);
	if(!character)
		return nullptr;

	CNetObj_PlayerInput newInput = *inputData;

	int startTick = Client()->GameTick(g_Config.m_ClDummy) + 1;
	int endTick = Client()->PredGameTick(g_Config.m_ClDummy) + predictTicks;

	for(int tick = startTick; tick <= endTick; ++tick)
	{
		predictWorld.m_GameTick++;
		character->OnPredictedInput(&newInput);
		predictWorld.Tick();
	}

	return character;
}

void CPlusFeatures::AutoWallshot()
{

	if(m_pClient->m_PredictedChar.m_IsInFreeze)
		return;

	auto& controls = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];
	int localId = m_pClient->m_Snap.m_LocalClientID;

	auto* pSetLaser = PredictTicks(9, &controls, localId);
	auto* pFreeze = PredictTicks(1, &controls, localId);
	auto* pTarget = PredictTicks(6, &controls, localId);

	if(!pSetLaser || !pFreeze || !pTarget)
		return;

	if(pSetLaser->m_FreezeTime > 0 && m_pClient->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_LASER)
	{
		controls.m_WantedWeapon = WEAPON_LASER;
	}

	if(pFreeze->Core()->m_IsInFreeze && m_pClient->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_LASER)
	{
		std::vector<WallshotLine> wallshotLines;
		GetLaserWallshot(wallshotLines, m_pClient->m_LocalCharacterPos, pTarget->m_Pos, 
			CCharacterCore::PhysicalSize(), 
			1);

		if(!wallshotLines.empty())
		{
			vec2 aimDir = wallshotLines[0].end - wallshotLines[0].start;
			controls.m_TargetX = static_cast<int>(aimDir.x);
			controls.m_TargetY = static_cast<int>(aimDir.y);
			controls.m_Fire = (controls.m_Fire + 2) % 64;
		}
	}
}

void CPlusFeatures::AutoAim()
{
	if(m_pClient->m_Snap.m_LocalClientID == -1 ||
		!m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_Active)
	{
		return;
	}

	int targetId = GetNearestPlayer(g_Config.m_ClPlusAutoAimRange);
	if(targetId < 0)
	{
		return;
	}

	auto *targetChar = m_pClient->m_PredictedWorld.GetCharacterByID(targetId);
	auto *localChar = m_pClient->m_PredictedWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);

	if(!localChar || !targetChar)
	{
		return;
	}

	CNetObj_PlayerInput curInput = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];

	const float accuracy = 0.031415928f;
	std::vector<vec2> bestDirs;

	float dx = targetChar->m_Pos.x - localChar->m_Pos.x;
	float dy = targetChar->m_Pos.y - localChar->m_Pos.y;
	float visibleAngle = atan2f(dy, dx) + 1.5707964f;

	for(float angle = visibleAngle; angle < visibleAngle + pi; angle += accuracy)
	{
		vec2 pos(
			targetChar->m_Pos.x + cosf(angle) * 28.0f,
			targetChar->m_Pos.y + sinf(angle) * 28.0f);

		vec2 scanDir = NormalizeAim(pos - localChar->m_Pos);

		CNetObj_PlayerInput aimbotInput = curInput;
		aimbotInput.m_Hook = 1;
		aimbotInput.m_TargetX = static_cast<int>(scanDir.x);
		aimbotInput.m_TargetY = static_cast<int>(scanDir.y);

		if(CanHook(10, &aimbotInput, &m_pClient->m_PredictedWorld))
		{
			bestDirs.push_back(scanDir);
		}
	}

	if(!bestDirs.empty())
	{
		Aim(bestDirs[0]);
		if(g_Config.m_ClPlusAutoAimAutofire)
		{
			m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire++;
		}
	}
}

void CPlusFeatures::Aim(vec2 dir)
{
	if(g_Config.m_ClPlusSilentAim)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetX = static_cast<int>(dir.x);
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetY = static_cast<int>(dir.y);
	}
	else
	{
		m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x = dir.x;
		m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y = dir.y;
	}
}

bool CPlusFeatures::CanHook(int predTicks, const CNetObj_PlayerInput *inputState, CGameWorld *baseWorld)
{
	if(!baseWorld)
		return false;

	CNetObj_PlayerInput inputData = *inputState;

	CGameWorld predictWorld;
	predictWorld.m_pCollision = baseWorld->m_pCollision;
	predictWorld.m_GameTick = baseWorld->m_GameTick;
	predictWorld.m_GameTickSpeed = baseWorld->m_GameTickSpeed;
	predictWorld.m_WorldConfig = baseWorld->m_WorldConfig;
	predictWorld.m_Teams = baseWorld->m_Teams;
	predictWorld.CopyWorld(baseWorld);

	CCharacter *localChar = predictWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);
	if(!localChar)
		return false;

	inputData.m_Hook = 1;
	bool hooked = false;

	for(int tick = 0; tick <= predTicks; ++tick)
	{
		if(localChar)
		{
			localChar->OnPredictedInput(&inputData);
		}

		predictWorld.m_GameTick++;
		predictWorld.Tick();

		if(localChar)
		{
			const CCharacterCore *core = localChar->Core();
			int hookedPlayer = core->HookedPlayer();

			if(hookedPlayer != -1)
			{
				if(core->m_HookState == 5)
			{
					hooked = true;
					break;
				}
			}
		}
	}

	return hooked;
}

vec2 CPlusFeatures::NormalizeAim(vec2 Pos)
{
	float len = ::length(Pos);
	if(len > 0)
	{
		Pos.x /= len;
		Pos.y /= len;
	}
	return Pos;
}

void CPlusFeatures::HammerCopy()
{
	auto *character = m_pClient->m_GameWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);
	if(!character)
	{
		return;
	}

	unsigned int targetId = character->pHammerHitId;
	if(targetId >= 64)
	{
		return;
	}

	const auto &targetData = m_pClient->m_aClients[targetId];

	if(g_Config.m_ClDummy == 1)
	{
		strncpy(g_Config.m_ClDummyName, targetData.m_aName, 14);
		int len = strlen(g_Config.m_ClDummyName);
		if(len >= 15)
		{
			g_Config.m_ClDummyName[15] = 0;
		}
		else
		{
			g_Config.m_ClDummyName[len] = '.';
			g_Config.m_ClDummyClan[len - 15] = 0;
		}
		g_Config.m_ClDummyColorBody = targetData.m_ColorBody;
		g_Config.m_ClDummyColorFeet = targetData.m_ColorFeet;
	}
	else if(g_Config.m_ClDummy == 0)
	{
		strncpy(g_Config.m_PlayerName, targetData.m_aName, 14);
		int len = strlen(g_Config.m_PlayerName);
		if(len >= 15)
		{
			g_Config.m_PlayerName[15] = 0;
		}
		else
		{
			g_Config.m_PlayerName[len] = '.';
			g_Config.m_PlayerClan[len - 15] = 0;
		}
		g_Config.m_ClPlayerColorBody = targetData.m_ColorBody;
		g_Config.m_ClPlayerColorFeet = targetData.m_ColorFeet;
	}

	character->pHammerHitId = -1;
}

void CPlusFeatures::HandleUnGrabFastHook()
{
	auto &inputData = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];
	const auto &localClient = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID];

	if(inputData.m_Hook)
	{
		if(localClient.m_Predicted.m_HookState != 4)
		{
			inputData.m_Hook = 0;
		}
	}
	else
	{
		inputData.m_Hook = 1;
	}
}

int CPlusFeatures::CheckPredictions(CNetObj_PlayerInput *inputData, int predictTicks, CGameWorld *curWorld)
{
	if(!curWorld)
	{
		return -1;
	}

	CGameWorld predictWorld(*curWorld);

	CCharacter *localChar = predictWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);
	if(!localChar)
	{
		return -1;
	}

	for(int tick = 0; tick <= predictTicks; ++tick)
	{
		localChar->OnPredictedInput(inputData);
		predictWorld.m_GameTick++;
		predictWorld.Tick();

		if(localChar->m_FreezeTime > 0)
		{
			return tick;
		}
	}

	return 9999;
}

void CPlusFeatures::GetLaserWallshot(std::vector<WallshotLine>& outLines, vec2 StartPos, vec2 TargetPos, float circleRadius, int bounce)
{
	float laserReach = m_pClient->m_aTuning[g_Config.m_ClDummy].m_LaserReach;

	vec2 main = StartPos;

	for(float angle = 0.0f; angle < 2.0f * pi; angle += 0.02f)
	{
		float nx = std::sin(angle);
		float ny = std::cos(angle);
		vec2 dir(nx, ny);

		vec2 headVector = dir;
		vec2 normalizedDir = normalize(dir);
		vec2 startPos = main + normalizedDir;

		std::vector<WallshotLine> linesX;
		GetLaserLines(&linesX, startPos, headVector, laserReach - 28.0f, bounce, 0);

		if(linesX.size() >= 2)
		{
			vec2 intersection1, intersection2;
			bool hasIntersection = GetLineCircleIntersection(
				TargetPos, circleRadius, &linesX[1],
				&intersection1, &intersection2);

			if(hasIntersection)
			{
				if(CheckPointIsOnLine(&linesX[1], intersection2))
				{
					outLines.push_back(linesX[0]);
				}
			}
		}
	}
}

void CPlusFeatures::GetLaserLines(std::vector<WallshotLine>* lines, vec2 start, vec2 dir, float reach, int bounce, int layer)
{
	std::vector<WallshotLine> linesTbl;
	float distanceTravel = reach;

	vec2 nDir = normalize(dir);
	vec2 lastLinePos = start;

	for(int it = 0; it <= bounce; ++it)
	{
		vec2 colPos(0, 0);
		vec2 beforeColPos(0, 0);
		vec2 nres = lastLinePos + nDir;

		if(Collision()->IntersectLine(lastLinePos, nres, &colPos, &beforeColPos))
		{
			vec2 movePoint = colPos;
			nDir = colPos - lastLinePos;
			Collision()->MovePoint(&movePoint, &nDir, 1.0f, nullptr);
			nres = beforeColPos;
		}

		if(layer <= 0 || it == layer)
		{
			linesTbl.push_back({lastLinePos, nres});
			if(it == layer)
				break;
		}

		float traveledDist = distance(lastLinePos, nres);
		if(traveledDist == 0.0f)
			break;

		distanceTravel -= traveledDist;
		nDir = normalize(nDir) * 0.0f;
		lastLinePos = nres;

		if(distanceTravel <= 0.0f)
			break;
	}

	*lines = linesTbl;
}

bool CPlusFeatures::GetLineCircleIntersection(
	vec2 center, float radius, const WallshotLine *line,
	vec2 *intersection1, vec2 *intersection2)
{
	float dx = line->end.x - line->start.x;
	float dy = line->end.y - line->start.y;
	float A = dx * dx + dy * dy;
	float B = 2.0f * (dx * (line->start.x - center.x) + dy * (line->start.y - center.y));
	float C = (line->start.x - center.x) * (line->start.x - center.x) +
		  (line->start.y - center.y) * (line->start.y - center.y) -
		  radius * radius;

	float det = B * B - 4 * A * C;

	if(A <= 0.0000001f || det < 0)
	{
		return false;
	}

	if(det == 0)
	{
		float t = -B / (2 * A);
		*intersection1 = vec2(
			line->start.x + t * dx,
			line->start.y + t * dy);
	}
	else
	{
		float t1 = (-B + std::sqrt(det)) / (2 * A);
		float t2 = (-B - std::sqrt(det)) / (2 * A);

		*intersection1 = vec2(
			line->start.x + t1 * dx,
			line->start.y + t1 * dy);
		*intersection2 = vec2(
			line->start.x + t2 * dx,
			line->start.y + t2 * dy);
	}

	return true;
}

bool CPlusFeatures::CheckPointIsOnLine(const WallshotLine *line, vec2 point)
{
	float distance1 = distance(vec2(line->start.x, line->start.y), point);
	float distance2 = distance(vec2(line->end.x, line->end.y), point);
	float lineDistance = distance(line->start, line->end);

	return std::fabs(distance1 + distance2 - lineDistance) < 0.0001f;
}

CNetObj_PlayerInput CPlusFeatures::FindBestInput(const std::vector<TestResult> &tests)
{
	CNetObj_PlayerInput bestInput{};
	int bestLifetime = -1;

	for(const auto &test : tests)
	{
		if(test.lifetime > bestLifetime)
		{
			bestInput = test.input;
			bestLifetime = test.lifetime;
		}

		if(bestLifetime == 9999)
		{
			break;
		}
	}

	return bestInput;
}

void CPlusFeatures::PredictionBot(int predictTicks, CGameWorld *curWorld)
{
	CNetObj_PlayerInput input = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];

	if(CheckPredictions(&input, 15, curWorld) == 9999)
	{
		return;
	}

	std::vector<int> dirs = {0, -1, 1};
	std::vector<TestResult> results;

	for(int dir : dirs)
	{
		for(int hook = 0; hook <= 1; ++hook)
		{
			CNetObj_PlayerInput testInput = input;
			testInput.m_Direction = dir;
			testInput.m_Hook = hook;

			TestResult result;
			result.lifetime = CheckPredictions(&testInput, predictTicks, curWorld);
			result.input = testInput;
			results.push_back(result);
		}
	}

	CNetObj_PlayerInput defaultInput{};
	CNetObj_PlayerInput newInput = GetAction(predictTicks, &results);

	if(m_ResultActions.back() >= predictTicks &&
		(newInput.m_Direction != defaultInput.m_Direction ||
			newInput.m_Hook != defaultInput.m_Hook ||
			newInput.m_Fire != defaultInput.m_Fire ||
			newInput.m_TargetX != defaultInput.m_TargetX))
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = newInput.m_Direction;
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Hook = newInput.m_Hook;
	}
}

void CPlusFeatures::FlyBot()
{
	auto *input = Input();

	if(input->KeyIsPressed(KEY_UP))
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos.y -= 1.0f;
	}
	else if(input->KeyIsPressed(KEY_DOWN))
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos.y += 1.0f;
	}

	else if(input->KeyIsPressed(KEY_LEFT))
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos.x -= 1.0f;
	}
	else if(input->KeyIsPressed(KEY_RIGHT))
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos.x += 1.0f;
	}

	if(!m_pClient->m_Controls.m_FlyInfo.Posed)
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos =
			m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Predicted.m_Pos;
	}
	m_pClient->m_Controls.m_FlyInfo.Posed = true;

	vec2 currentPos = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Predicted.m_Pos;
	if(::length(currentPos - m_pClient->m_Controls.m_FlyInfo.PrevPos) > 50.0f)
	{
		m_pClient->m_Controls.m_FlyInfo.PrevPos = currentPos;
	}

	if(currentPos.y <= m_pClient->m_Controls.m_FlyInfo.PrevPos.y)
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Hook = 0;
	}
	else
	{
		HandleUnGrabFastHook();
	}

	m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction =
		(m_pClient->m_Controls.m_FlyInfo.PrevPos.x <= currentPos.x) ? -1 : 1;
}

void CPlusFeatures::AntiFallingFreeze()
{
	vec2 pos = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Predicted.m_Pos;
	vec2 vel = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Predicted.m_Vel;

	int x = static_cast<int>(pos.x);
	int y = static_cast<int>(pos.y);

	vec2 futurePos(x, y + (32.0f * vel.y) / 15.0f);
	int indexFuture = m_pClient->Collision()->GetPureMapIndex(futurePos);
	int indexCurrent = m_pClient->Collision()->GetPureMapIndex(pos);

	int tileFuture = m_pClient->Collision()->GetTileIndex(indexFuture);
	int tileCurrent = m_pClient->Collision()->GetTileIndex(indexCurrent);

	if(tileFuture == TILE_FREEZE &&
		(tileFuture != TILE_FREEZE || tileCurrent != TILE_FREEZE))
	{
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump = 1;
		NeedReset = true;
	}
}

void CPlusFeatures::AntiGoingFreeze(int type)
{
	if(!m_pClient->m_Snap.m_pLocalCharacter &&
			!m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_LocalClientID].m_Active ||
		m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team == -1)
	{
		return;
	}

	vec2 pos = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Predicted.m_Pos;
	int x = static_cast<int>(pos.x);
	int y = static_cast<int>(pos.y);
	int range = g_Config.m_ClPlusAntiGoingFreezeRange;

	if(type == 1)
	{
		vec2 rightPos(x + range, y);
		int indexRight = m_pClient->Collision()->GetPureMapIndex(rightPos);
		if(m_pClient->Collision()->GetTileIndex(indexRight) == TILE_FREEZE)
		{
			m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = -1;
		}
		else
		{
			m_pClient->m_Controls.m_aLastData[g_Config.m_ClDummy].m_Direction = 0;
		}

		vec2 leftPos(x - range, y);
		int indexLeft = m_pClient->Collision()->GetPureMapIndex(leftPos);
		if(m_pClient->Collision()->GetTileIndex(indexLeft) == TILE_FREEZE)
		{
			m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = 1;
		}
		else
		{
			m_pClient->m_Controls.m_aLastData[g_Config.m_ClDummy].m_Direction = 0;
		}
	}
	else if(type == 2)
	{
		vec2 rightPos(x + range, y);
		int indexRight = m_pClient->Collision()->GetPureMapIndex(rightPos);
		if(m_pClient->Collision()->GetTileIndex(indexRight) == TILE_FREEZE)
		{
			if(m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction == 1)
			{
				m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = 0;
			}
		}
		else
		{
			m_pClient->m_Controls.m_aLastData[g_Config.m_ClDummy].m_Direction =
				m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction;
		}

		vec2 leftPos(x - range, y);
		int indexLeft = m_pClient->Collision()->GetPureMapIndex(leftPos);
		if(m_pClient->Collision()->GetTileIndex(indexLeft) == TILE_FREEZE)
		{
			if(m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction == -1)
			{
				m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = 0;
			}
		}
		else
		{
			m_pClient->m_Controls.m_aLastData[g_Config.m_ClDummy].m_Direction =
				m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction;
		}
	}
}

CNetObj_PlayerInput CPlusFeatures::GetAction(int ticks, std::vector<TestResult> *results)
{
	CNetObj_PlayerInput bestInput{};
	int bestLifetime = -1;

	for(const auto &result : *results)
	{
		if(result.lifetime > bestLifetime)
		{
			bestInput = result.input;
			bestLifetime = result.lifetime;
		}

		if(bestLifetime == 9999)
		{
			break;
		}
	}

	m_ResultActions.push_back(bestLifetime);
	return bestInput;
}