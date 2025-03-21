#include <base/system.h>
#include <engine/discord.h>

#if defined(CONF_DISCORD)
#include <discord_game_sdk.h>

typedef enum EDiscordResult(DISCORD_API *FDiscordCreate)(DiscordVersion, struct DiscordCreateParams *, struct IDiscordCore **);

#if defined(CONF_DISCORD_DYNAMIC)
#include <dlfcn.h>
FDiscordCreate GetDiscordCreate()
{
	void *pSdk = dlopen("discord_game_sdk.so", RTLD_NOW);
	if(!pSdk)
	{
		return nullptr;
	}
	return (FDiscordCreate)dlsym(pSdk, "DiscordCreate");
}
#else
FDiscordCreate GetDiscordCreate()
{
	return DiscordCreate;
}
#endif

class CDiscord : public IDiscord
{
	IDiscordCore *m_pCore;
	IDiscordActivityEvents m_ActivityEvents;
	IDiscordActivityManager *m_pActivityManager;
	bool m_Enabled;
	FDiscordCreate m_pfnDiscordCreate;

public:
	int64_t m_TimeStamp = time_timestamp(); 
	bool Init(FDiscordCreate pfnDiscordCreate)
	{
		m_pfnDiscordCreate = pfnDiscordCreate;
		m_Enabled = false;
		return InitDiscord();
	}
	bool InitDiscord()
	{
		if(m_pCore)
		{
			m_pCore->destroy(m_pCore);
			m_pCore = 0;
			m_pActivityManager = 0;
		}
		if(!m_Enabled)
			return false;

		mem_zero(&m_ActivityEvents, sizeof(m_ActivityEvents));
		m_pActivityManager = 0;

		DiscordCreateParams Params;
		DiscordCreateParamsSetDefault(&Params);

		Params.client_id = 1325507236331524116; // A-Client
		Params.flags = EDiscordCreateFlags::DiscordCreateFlags_NoRequireDiscord;
		Params.event_data = this;
		Params.activity_events = &m_ActivityEvents;
		int Error = m_pfnDiscordCreate(DISCORD_VERSION, &Params, &m_pCore);
		if(Error != DiscordResult_Ok)
		{
			dbg_msg("discord", "error initializing discord instance, error=%d", Error);
			return true;
		}

		m_pActivityManager = m_pCore->get_activity_manager(m_pCore);
		return false;
	}
	void Update(bool Enabled) override
	{
		bool NeedsUpdate = m_Enabled != Enabled;
		m_Enabled = Enabled;

		if(NeedsUpdate)
			InitDiscord();

		if(m_pCore && m_Enabled)
			m_pCore->run_callbacks(m_pCore);
	}
	void ClearGameInfo(const char *pDetail) override
	{
		if(!m_Enabled || !m_pActivityManager)
			return;

		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		str_copy(Activity.assets.large_image, "ac_image_b_o", sizeof(Activity.assets.large_image));
		str_copy(Activity.assets.large_text, "github.com/qxdFox/Aiodob-Client-DDNet", sizeof(Activity.assets.large_text));
		Activity.timestamps.start = m_TimeStamp; // Discord is forcing TimeStamps now?
		str_copy(Activity.details, pDetail, sizeof(Activity.details));
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, const char *pDetail, bool ShowMap, bool AnnounceAddr) override
	{
		if(!m_Enabled || !m_pActivityManager)
			return;

		DiscordActivity Activity;
		mem_zero(&Activity, sizeof(DiscordActivity));
		str_copy(Activity.assets.large_image, "ac_image_b", sizeof(Activity.assets.large_image));
		str_copy(Activity.assets.large_text, "github.com/qxdFox/Aiodob-Client-DDNet", sizeof(Activity.assets.large_text));
		Activity.timestamps.start = m_TimeStamp; // Discord is forcing TimeStamps now?
		str_copy(Activity.details, pDetail, sizeof(Activity.details));
		if(ShowMap)
			str_copy(Activity.state, pMapName, sizeof(Activity.state));
		m_pActivityManager->update_activity(m_pActivityManager, &Activity, 0, 0);
	}
	~CDiscord()
	{
		if(m_pCore)
			m_pCore->destroy(m_pCore);
	}
};

IDiscord *CreateDiscordImpl()
{
	FDiscordCreate pfnDiscordCreate = GetDiscordCreate();
	if(!pfnDiscordCreate)
	{
		return 0;
	}
	CDiscord *pDiscord = new CDiscord();
	if(pDiscord->Init(pfnDiscordCreate))
	{
		delete pDiscord;
		return 0;
	}
	return pDiscord;
}
#else
IDiscord *CreateDiscordImpl()
{
	return nullptr;
}
#endif

class CDiscordStub : public IDiscord
{
	void Update(bool RPC) override {}
	void ClearGameInfo(const char *pDetail) override {}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, const char *pDetail, bool ShowMap,bool AnnounceAddr) override {}
};

IDiscord *CreateDiscord()
{
	IDiscord *pDiscord = CreateDiscordImpl();
	if(pDiscord)
	{
		return pDiscord;
	}
	return new CDiscordStub();
}