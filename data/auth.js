(function(window) {
  const STORAGE_KEY = "artnetAdminPassword";
  const USERNAME = "admin";
  let password = localStorage.getItem(STORAGE_KEY) || "";

  function persist(value) {
    password = (value || "").trim();
    if (password) {
      localStorage.setItem(STORAGE_KEY, password);
    } else {
      localStorage.removeItem(STORAGE_KEY);
    }
  }

  async function request(resource, options = {}, retry = true) {
    const opts = Object.assign({}, options);
    opts.headers = new Headers(options && options.headers ? options.headers : {});
    const authHeader = getAuthHeader();
    if (authHeader) {
      opts.headers.set("Authorization", authHeader);
    }

    const response = await fetch(resource, opts);
    if (response.status === 401) {
      if (!retry) {
        throw new Error("Authentication failed. Please check your admin password.");
      }
      const input = window.prompt("Admin password required");
      if (input === null) {
        throw new Error("Authentication cancelled by user");
      }
      persist(input);
      return request(resource, options, false);
    }
    return response;
  }

  function getAuthHeader() {
    if (!password) {
      return null;
    }
    return "Basic " + btoa(`${USERNAME}:${password}`);
  }

  window.AuthClient = {
    request,
    setPassword: persist,
    clearPassword: () => persist(""),
    getPassword: () => password,
    hasPassword: () => !!password,
    getAuthHeader
  };
})(window);
