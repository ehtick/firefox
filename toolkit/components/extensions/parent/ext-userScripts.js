/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionUserScripts: "resource://gre/modules/ExtensionUserScripts.sys.mjs",
});

var { ExtensionUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUtils.sys.mjs"
);

var { ExtensionError } = ExtensionUtils;

/**
 * Represents (in the main browser process) a user script.
 * (legacy MV2-only userScripts API)
 *
 * @param {UserScriptOptions} details
 *        The options object related to the user script
 *        (which has the properties described in the user_scripts.json
 *        JSON API schema file).
 */
class UserScriptParent {
  constructor(details) {
    this.scriptId = details.scriptId;
    this.options = this._convertOptions(details);
  }

  destroy() {
    if (this.destroyed) {
      throw new Error("Unable to destroy UserScriptParent twice");
    }

    this.destroyed = true;
    this.options = null;
  }

  _convertOptions(details) {
    const options = {
      matches: details.matches,
      excludeMatches: details.excludeMatches,
      includeGlobs: details.includeGlobs,
      excludeGlobs: details.excludeGlobs,
      allFrames: details.allFrames,
      matchAboutBlank: details.matchAboutBlank,
      // New matchOriginAsFallback option not supported in userScripts API
      // because the current MV2-only userScripts API is deprecated and will be
      // superseded by the new one in bug 1875475.
      matchOriginAsFallback: false,
      runAt: details.runAt || "document_idle",
      // "world" option is unsupported in the old userScripts API. The new one
      // (bug 1875475) will support "USER_SCRIPT" (default) and "MAIN".
      jsPaths: details.js,
      userScriptOptions: {
        scriptMetadata: details.scriptMetadata,
      },
      originAttributesPatterns: null,
    };

    if (details.cookieStoreId != null) {
      const cookieStoreIds = Array.isArray(details.cookieStoreId)
        ? details.cookieStoreId
        : [details.cookieStoreId];
      options.originAttributesPatterns = cookieStoreIds.map(cookieStoreId =>
        getOriginAttributesPatternForCookieStoreId(cookieStoreId)
      );
    }

    return options;
  }

  serialize() {
    return this.options;
  }
}

this.userScripts = class extends ExtensionAPI {
  constructor(...args) {
    super(...args);

    // Map<scriptId -> UserScriptParent>
    this.userScriptsMap = new Map();
  }

  getAPI(context) {
    const { extension } = context;
    if (extension.manifestVersion == 2) {
      return this.getLegacyMV2API(context);
    }

    function ensureIdsValidAndUnique(publicScriptIds) {
      let seen = new Set();
      for (let id of publicScriptIds) {
        if (!id.length || id.startsWith("_")) {
          throw new ExtensionError("Invalid id for RegisteredUserScript.");
        }
        if (seen.has(id)) {
          throw new ExtensionError(`Duplicate script id: ${id}`);
        }
        seen.add(id);
      }
    }

    function ensureValidWorldId(worldId) {
      if (worldId && (worldId.startsWith("_") || worldId.length > 256)) {
        // worldId "" is the default world.
        // worldId starting with "_" are reserved.
        // worldId length is capped. Limit is consistent with Chrome.
        throw new ExtensionError(`Invalid worldId: ${worldId}`);
      }
    }

    if (!extension.userScriptsManager) {
      // extension.userScriptsManager is initialized by initExtension() at
      // extension startup when the extension has the "userScripts" permission.
      // When we get here, it means that "userScripts" was requested after
      // startup, and we need to initialize it here.
      ExtensionUserScripts.initExtension(extension);
    }

    const usm = extension.userScriptsManager;

    return {
      userScripts: {
        register: async scripts => {
          ensureIdsValidAndUnique(scripts.map(s => s.id));
          scripts.forEach(s => ensureValidWorldId(s.worldId));

          return usm.runWriteTask(async () => {
            await usm.registerNewScripts(scripts);
          });
        },

        update: async scripts => {
          ensureIdsValidAndUnique(scripts.map(s => s.id));
          scripts.forEach(s => ensureValidWorldId(s.worldId));

          return usm.runWriteTask(async () => {
            await usm.updateScripts(scripts);
          });
        },

        unregister: async filter => {
          let ids = filter?.ids;
          if (ids) {
            ensureIdsValidAndUnique(ids);
          }
          return usm.runWriteTask(async () => {
            await usm.unregisterScripts(ids);
          });
        },

        getScripts: async filter => {
          let ids = filter?.ids;
          if (ids) {
            ensureIdsValidAndUnique(ids);
          }
          return usm.runReadTask(async () => {
            return usm.getScripts(ids);
          });
        },

        configureWorld: async properties => {
          ensureValidWorldId(properties.worldId);
          return usm.runWriteTask(async () => {
            await usm.configureWorld(properties);
          });
        },

        resetWorldConfiguration: async worldId => {
          ensureValidWorldId(worldId);
          return usm.runWriteTask(async () => {
            await usm.resetWorldConfiguration(worldId);
          });
        },

        getWorldConfigurations: async () => {
          return usm.runReadTask(async () => {
            return usm.getWorldConfigurations();
          });
        },
      },
    };
  }

  getLegacyMV2API(context) {
    const { extension } = context;

    // Set of the scriptIds registered from this context.
    const registeredScriptIds = new Set();

    const unregisterContentScripts = scriptIds => {
      if (scriptIds.length === 0) {
        return Promise.resolve();
      }

      for (let scriptId of scriptIds) {
        registeredScriptIds.delete(scriptId);
        extension.registeredContentScripts.delete(scriptId);
        this.userScriptsMap.delete(scriptId);
      }
      extension.updateContentScripts();

      return context.extension.broadcast("Extension:UnregisterContentScripts", {
        id: context.extension.id,
        scriptIds,
      });
    };

    // Unregister all the scriptId related to a context when it is closed,
    // and revoke all the created blob urls once the context is destroyed.
    context.callOnClose({
      close() {
        unregisterContentScripts(Array.from(registeredScriptIds));
      },
    });

    return {
      userScripts: {
        register: async details => {
          for (let origin of details.matches) {
            if (!extension.allowedOrigins.subsumes(new MatchPattern(origin))) {
              throw new ExtensionError(
                `Permission denied to register a user script for ${origin}`
              );
            }
          }

          const userScript = new UserScriptParent(details);
          const { scriptId } = userScript;

          this.userScriptsMap.set(scriptId, userScript);
          registeredScriptIds.add(scriptId);

          const scriptOptions = userScript.serialize();

          extension.registeredContentScripts.set(scriptId, scriptOptions);
          extension.updateContentScripts();

          await extension.broadcast("Extension:RegisterContentScripts", {
            id: extension.id,
            scripts: [{ scriptId, options: scriptOptions }],
          });

          return scriptId;
        },

        // This method is not available to the extension code, the extension code
        // doesn't have access to the internally used scriptId, on the contrary
        // the extension code will call script.unregister on the script API object
        // that is resolved from the register API method returned promise.
        unregister: async scriptId => {
          const userScript = this.userScriptsMap.get(scriptId);
          if (!userScript) {
            throw new Error(`No such user script ID: ${scriptId}`);
          }

          userScript.destroy();

          await unregisterContentScripts([scriptId]);
        },
      },
    };
  }
};
