#include "guis/GuiRandomCollectionOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "guis/GuiSettings.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "SystemData.h"
#include "Window.h"

GuiRandomCollectionOptions::GuiRandomCollectionOptions(Window* window) : GuiComponent(window), mMenu(window, "RANDOM COLLECTION")
{
	customCollectionLists.clear();
	autoCollectionLists.clear();
	systemLists.clear();
	mNeedsCollectionRefresh = false;

	initializeMenu();
}

void GuiRandomCollectionOptions::initializeMenu()
{
	// get collections
	addEntry("INCLUDE SYSTEMS", 0x777777FF, true, [this] { selectSystems(); });
	addEntry("INCLUDE AUTO COLLECTIONS", 0x777777FF, true, [this] { selectAutoCollections(); });
	addEntry("INCLUDE CUSTOM COLLECTIONS", 0x777777FF, true, [this] { selectCustomCollections(); });

	// Add option to exclude games from a collection
	exclusionCollection = std::make_shared< OptionListComponent<std::string> >(mWindow, "EXCLUDE GAMES FROM", false);

	// Add default option
	exclusionCollection->add("<NONE>", "", Settings::getInstance()->getString("RandomCollectionExclusionCollection") == "");

	std::map<std::string, CollectionSystemData> customSystems = CollectionSystemManager::get()->getCustomCollectionSystems();
	// add all enabled Custom Systems
	for(std::map<std::string, CollectionSystemData>::const_iterator it = customSystems.cbegin() ; it != customSystems.cend() ; it++ )
	{
		exclusionCollection->add(it->second.decl.longName, it->second.decl.name, Settings::getInstance()->getString("RandomCollectionExclusionCollection") == it->second.decl.name);
	}

	mMenu.addWithLabel("EXCLUDE GAMES FROM", exclusionCollection);

	// Add option to trim random collection items
	trimRandom = std::make_shared< OptionListComponent<std::string> >(mWindow, "MAX GAMES", false);

	// Add default entry
	trimRandom->add("ALL", "", Settings::getInstance()->getString("RandomCollectionMaxItems") == "");

	// add limit values for size of random collection
	for(int i = 5; i <= 50; i = i+5)
	{
		trimRandom->add(std::to_string(i), std::to_string(i), Settings::getInstance()->getString("RandomCollectionMaxItems") == std::to_string(i));
	}

	mMenu.addWithLabel("MAX GAMES", trimRandom);

	addChild(&mMenu);

	mMenu.addButton("OK", "ok", std::bind(&GuiRandomCollectionOptions::saveSettings, this));
	mMenu.addButton("CANCEL", "cancel", [&] { delete this; });

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiRandomCollectionOptions::addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if(add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

void GuiRandomCollectionOptions::selectSystems()
{
	std::map<std::string, CollectionSystemData> systems;
	for(auto &sys : SystemData::sSystemVector)
	{
		// we won't iterate all collections
		if (sys->isGameSystem() && !sys->isCollection())
		{
			CollectionSystemDecl sysDecl;
			sysDecl.name = sys->getName();
			sysDecl.longName = sys->getFullName();

			CollectionSystemData newCollectionData;
			newCollectionData.system = sys;
			newCollectionData.decl = sysDecl;
			newCollectionData.isEnabled = true;

			systems[sysDecl.name] = newCollectionData;
		}
	}
	selectEntries(systems, "RandomCollectionSystems", DEFAULT_RANDOM_SYSTEM_GAMES, &systemLists);
}

void GuiRandomCollectionOptions::selectAutoCollections()
{
	selectEntries(CollectionSystemManager::get()->getAutoCollectionSystems(), "RandomCollectionSystemsAuto", DEFAULT_RANDOM_COLLECTIONS_GAMES, &autoCollectionLists);
}

void GuiRandomCollectionOptions::selectCustomCollections()
{
	selectEntries(CollectionSystemManager::get()->getCustomCollectionSystems(), "RandomCollectionSystemsCustom", DEFAULT_RANDOM_COLLECTIONS_GAMES, &customCollectionLists);
}

GuiRandomCollectionOptions::~GuiRandomCollectionOptions()
{

}

void GuiRandomCollectionOptions::selectEntries(std::map<std::string, CollectionSystemData> collection, std::string settingsLabel, int defaultValue, std::vector< SystemGames>* results) {
	auto s = new GuiSettings(mWindow, "INCLUDE GAMES FROM");

	std::map<std::string, std::any> initValues = Settings::getInstance()->getMap(settingsLabel);

	results->clear();

	for(auto &[_, csd] : collection)
	{
		if (csd.system != CollectionSystemManager::get()->getRandomCollection())
		{
			ComponentListRow row;

			std::string label = csd.decl.longName;
			int selectedValue = defaultValue;

			if (initValues.find(label) != initValues.end())
			{
				int v = std::any_cast<int>(initValues[label]);
				// we won't add more than the max and less than 0
				selectedValue = Math::max(Math::min(RANDOM_SYSTEM_MAX, v), 0);
				mNeedsCollectionRefresh |= selectedValue != v;
			}

			initValues[label] = selectedValue;

			auto colItems = std::make_shared<SliderComponent>(mWindow, 0.f, float(RANDOM_SYSTEM_MAX), 1.f, " " /* space as 'unit' to have amount rendered */);
			colItems->setValue((float)selectedValue);
			row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
			row.addElement(colItems, false);

			s->addRow(row);
			SystemGames sys;
			sys.name = label;
			sys.gamesSelection = colItems;
			results->push_back(sys);
		}

	}

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	s->setPosition((mSize.x() - s->getSize().x()) / 2, (mSize.y() - s->getSize().y()) / 2);
	s->addSaveFunc([this, settingsLabel, initValues, results] { applyGroupSettings(settingsLabel, initValues, results); });
	mWindow->pushGui(s);
}


template <typename Map>
bool GuiRandomCollectionOptions::equal(Map const &_this, Map const &_that)
{
    return _this.size() == _that.size() // map key count
		&& std::equal(_this.begin(), _this.end(), _that.begin(), [] (auto a, auto b) { return a.first == b.first; }) // map keys
		&& std::equal(_this.begin(), _this.end(), _that.begin(), [] (auto a, auto b) { return std::any_cast<int>(a.second) == std::any_cast<int>(b.second); }); // map values
}


void GuiRandomCollectionOptions::applyGroupSettings(std::string settingsLabel, const std::map<std::string, std::any> &initialValues, std::vector<SystemGames> *results)
{
	std::map<std::string,std::any> currentValues;
	for (auto it = results->begin(); it != results->end(); ++it)
	{
		currentValues[(*it).name] = int((*it).gamesSelection->getValue());
	}
	if (!equal(currentValues, initialValues))
	{
		mNeedsCollectionRefresh = true;
		Settings::getInstance()->setMap(settingsLabel, currentValues);
	}
}

void GuiRandomCollectionOptions::saveSettings()
{
	std::string curTrim = trimRandom->getSelected();
	std::string prevTrim = Settings::getInstance()->getString("RandomCollectionMaxItems");
	Settings::getInstance()->setString("RandomCollectionMaxItems", curTrim);

	std::string curExclusion = exclusionCollection->getSelected();
	std::string prevExclusion = Settings::getInstance()->getString("RandomCollectionExclusionCollection");
	Settings::getInstance()->setString("RandomCollectionExclusionCollection", curExclusion);

	mNeedsCollectionRefresh |= (curTrim != prevTrim || curExclusion != prevExclusion);

	if (mNeedsCollectionRefresh)
	{
		Settings::getInstance()->saveFile();
		CollectionSystemManager::get()->recreateCollection(CollectionSystemManager::get()->getRandomCollection());
	}

	delete this;
}

bool GuiRandomCollectionOptions::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;

	if(config->isMappedTo("b", input) && input.value != 0)
		saveSettings();

	return false;
}

std::vector<HelpPrompt> GuiRandomCollectionOptions::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("b", "back"));
	return prompts;
}