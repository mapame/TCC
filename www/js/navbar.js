function addEventsNavbarBurgers() {
	for(burguerElement of document.getElementsByClassName("navbar-burger")) {
		burguerElement.onclick = burguerNavbarToggle;
		burguerElement.appendChild(document.createElement("span"));
		burguerElement.appendChild(document.createElement("span"));
		burguerElement.appendChild(document.createElement("span"));
	}
}

function burguerNavbarToggle() {
	document.getElementById(this.dataset.target).classList.toggle("is-active");
	this.classList.toggle("is-active");
}

function navbarPopulateItems(navbarContainerId) {
	var navbarMenu;
	var navbarStart;
	var navbarEnd;
	
	if(typeof navbarContainerId !== "string" || (navbarMenu = document.getElementById(navbarContainerId)) === null)
		return;
	
	navbarStart = document.createElement("div");
	navbarEnd = document.createElement("div");
	
	navbarMenu.appendChild(navbarStart);
	navbarMenu.appendChild(navbarEnd);
	
	navbarStart.className = "navbar-start";
	navbarEnd.className = "navbar-end";
	
	if(typeof window.smceeNavbarMenuItems === "object") {
		for(menuItem of window.smceeNavbarMenuItems) {
			let menuItemElement;
			
			if(typeof menuItem.name !== "string" || (typeof menuItem.adminOnly === "boolean" && menuItem.adminOnly === true && loggedUserInfo.is_admin === false))
				continue;
			
			if(typeof menuItem.dropdown === "object" && menuItem.dropdown.length > 1) {
				let dropdownLink = document.createElement("a");
				let dropdownContainer = document.createElement("div");
				
				menuItemElement = document.createElement("div");
				
				menuItemElement.appendChild(dropdownLink);
				menuItemElement.appendChild(dropdownContainer);
				
				dropdownLink.className = "navbar-link";
				dropdownLink.innerText = menuItem.name;
				
				dropdownContainer.className = "navbar-dropdown";
				
				menuItemElement.className = "navbar-item has-dropdown is-hoverable";
				
				for(dropdownItem of menuItem.dropdown) {
					let dropdownItemElement
					
					if(typeof dropdownItem !== "object" || (typeof dropdownItem.adminOnly === "boolean" && dropdownItem.adminOnly === true && loggedUserInfo.is_admin === false))
						continue;
					
					if(typeof dropdownItem.name === "string" && typeof dropdownItem.href === "string") {
						dropdownItemElement = document.createElement("a");
						
						dropdownItemElement.className = "navbar-item";
						
						dropdownItemElement.innerText = dropdownItem.name;
						
						if(typeof dropdownItem.href === "string")
							dropdownItemElement.href = dropdownItem.href;
						
					} else if(typeof dropdownItem.name === "object" && dropdownItem.name === null) {
						dropdownItemElement = document.createElement("hr");
						dropdownItemElement.className = "navbar-divider";
					}
					
					dropdownContainer.appendChild(dropdownItemElement);
				}
			} else {
				menuItemElement = document.createElement("a");
				
				menuItemElement.className = "navbar-item";
				
				menuItemElement.innerText = menuItem.name;
				
				if(typeof menuItem.href === "string")
					menuItemElement.href = menuItem.href;
			}
			
			navbarStart.appendChild(menuItemElement);
		}
	}
	
	if(typeof window.loggedUserInfo === "object" && window.loggedUserInfo !== null) {
		let usernameNavbarItem = document.createElement("div");
		let usernameContainer = document.createElement("div");
		let usernameTag = document.createElement("a");
		let logoutButton = document.createElement("a");
		
		navbarEnd.appendChild(usernameNavbarItem);
		
		usernameNavbarItem.appendChild(usernameContainer);
		
		usernameContainer.appendChild(usernameTag);
		usernameContainer.appendChild(logoutButton);
		
		usernameNavbarItem.className = "navbar-item";
		
		usernameContainer.className = "tags has-addons";
		usernameTag.className = "tag is-rounded is-light has-text-weight-bold";
		logoutButton.className = "tag is-delete is-rounded is-light is-danger";
		
		usernameTag.href = "users.html";
		
		logoutButton.addEventListener("click", authLogout);
		
		usernameTag.innerText = window.loggedUserInfo.name + (window.loggedUserInfo.is_admin ? " (administrador)" : "");
	}
}
