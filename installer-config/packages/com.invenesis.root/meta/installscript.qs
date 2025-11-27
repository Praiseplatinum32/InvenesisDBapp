// installscript.qs
// Runs extra operations when com.invenesis.root is installed.

function Component()
{
    // default constructor
}

// Called by IFW during installation to set up operations
Component.prototype.createOperations = function()
{
    // First, let IFW do its normal work (copy files, etc.)
    component.createOperations();

    // Only interesting on Windows for environment variables
    if (installer.value("os") !== "win") {
        // On non-Windows we currently do nothing
        return;
    }

    // - host (10.0.0.19 is only inside your LAN)
    // - port
    // - database name
    // - db user
    //
    // They will be stored as *per-user* persistent env vars.
    // (persistent = "true", no 'system' argument)

    component.addOperation("EnvironmentVariable",
                           "INV_DB_HOST",
                           "10.0.0.19",
                           "true");

    component.addOperation("EnvironmentVariable",
                           "INV_DB_PORT",
                           "5432",
                           "true");

    component.addOperation("EnvironmentVariable",
                           "INV_DB_NAME",
                           "invenesisdb",
                           "true");

    component.addOperation("EnvironmentVariable",
                           "INV_DB_USER",
                           "invenesis_app",
                           "true");

    // --- IMPORTANT: password is NOT set here ---

}
