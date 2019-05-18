/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2019-01-30
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <Game/GameController.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <memory>
#include <vector>

/*
 * This class manages UI preferences,and takes care of persisting and loading them.
 *
 * The class also serves as the storage for some of the preferences.
 */
class UIPreferencesManager
{
public:

    UIPreferencesManager(std::shared_ptr<GameController> gameController);

    ~UIPreferencesManager();

public:

    std::vector<std::filesystem::path> const & GetShipLoadDirectories() const
    {
        return mShipLoadDirectories;
    }

    void AddShipLoadDirectory(std::filesystem::path shipLoadDirectory)
    {
        // We always have the default ship directory in the first position
        assert(mShipLoadDirectories.size() >= 1);

        if (shipLoadDirectory != mShipLoadDirectories[0])
        {
            // Check if we have one already
            for (auto it = mShipLoadDirectories.begin(); it != mShipLoadDirectories.end(); ++it)
            {
                if (*it == shipLoadDirectory)
                {
                    // Move it to second place
                    std::rotate(mShipLoadDirectories.begin() + 1, it, it + 1);

                    return;
                }
            }

            // Add to second place
            mShipLoadDirectories.insert(mShipLoadDirectories.begin() + 1, shipLoadDirectory);
        }
    }

    std::filesystem::path const & GetScreenshotsFolderPath() const
    {
        return mScreenshotsFolderPath;
    }

    void SetScreenshotsFolderPath(std::filesystem::path screenshotsFolderPath)
    {
        mScreenshotsFolderPath = std::move(screenshotsFolderPath);
    }

    bool GetShowStartupTip() const
    {
        return mShowStartupTip;
    }

    void SetShowStartupTip(bool value)
    {
        mShowStartupTip = value;
    }

    bool GetShowShipDescriptionsAtShipLoad() const
    {
        return mShowShipDescriptionsAtShipLoad;
    }

    void SetShowShipDescriptionsAtShipLoad(bool value)
    {
        mShowShipDescriptionsAtShipLoad = value;
    }

    bool GetShowTsunamiNotifications() const
    {
        return mGameController->GetShowTsunamiNotifications();
    }

    void SetShowTsunamiNotifications(bool value)
    {
        mGameController->SetShowTsunamiNotifications(value);
    }

private:

    void LoadPreferences();

    void SavePreferences() const;

private:

    std::filesystem::path const mDefaultShipLoadDirectory;

    // The owners/storage of our properties
    std::shared_ptr<GameController> mGameController;

    //
    // The preferences for which we are the owners/storage

    std::vector<std::filesystem::path> mShipLoadDirectories;

    std::filesystem::path mScreenshotsFolderPath;

    bool mShowStartupTip;
    bool mShowShipDescriptionsAtShipLoad;
};
