/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket.interactor

import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory

/**
 * Contract for all possible user interactions with the Pocket recommended stories feature.
 */
interface PocketStoriesInteractor {
    /**
     * Callback for when a certain story is shown to the user.
     *
     * @param storyShown The story shown to the user.
     * @param storyPosition `row x column x index` matrix representing the grid and index position
     * of the shown story.
     */
    fun onStoryShown(storyShown: PocketStory, storyPosition: Triple<Int, Int, Int>)

    /**
     * Callback for then new stories are shown to the user.
     *
     * @param storiesShown The new list of [PocketRecommendedStory]es shown to the user.
     */
    fun onStoriesShown(storiesShown: List<PocketStory>)

    /**
     * Callback for when the user clicks a specific category.
     *
     * @param categoryClicked The just clicked [PocketRecommendedStoriesCategory].
     */
    fun onCategoryClicked(categoryClicked: PocketRecommendedStoriesCategory)

    /**
     * Callback for when the user clicks on a specific story.
     *
     * @param storyClicked The just clicked [PocketStory].
     * @param storyPosition `row x column x index` matrix representing the grid and index position
     * of the clicked story.
     */
    fun onStoryClicked(storyClicked: PocketStory, storyPosition: Triple<Int, Int, Int>)
}
