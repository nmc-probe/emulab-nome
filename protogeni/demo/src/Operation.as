package
{
  import flash.events.ErrorEvent;
  import flash.events.Event;
  import com.mattism.http.xmlrpc.ConnectionImpl;

  class Operation
  {
    public function Operation(newModule : String, newMethod : String) : void
    {
      server = null;
      reset(newModule, newMethod);
    }

    public function reset(newModule : String, newMethod : String) : void
    {
      module = newModule;
      method = newMethod;
      clearFields();
      resetUrl();
      if (server != null)
      {
        server.cleanup();
        server = null;
      }
      success = null;
      failure = null;
    }

    public function resetUrl() : void
    {
      url = "https://" + XMLRPC_SERVER + SERVER_PATH + module;
    }

    public function setUrl(newUrl : String) : void
    {
      url = newUrl + "/" + module;
    }

    public function addField(key : String, value : Object) : void
    {
      param[key] = value;
    }

    public function clearFields() : void
    {
      param = new Object();
    }

    public function call(newSuccess : Function, newFailure : Function) : void
    {
      success = newSuccess;
      failure = newFailure;
      server = new ConnectionImpl(url);
      server.addEventListener(Event.COMPLETE, callSuccess);
      server.addEventListener(ErrorEvent.ERROR, callFailure);
      server.addParam(param, "struct");
      server.call(method);
    }

    public function getSendXml() : String
    {
      return server._method.getXml().toString();
    }

    public function getResponseXml() : String
    {
      return String(server._response.data);
    }

    function callSuccess(event : Event) : void
    {
      cleanup();
      success(Number(server.getResponse().code),
//              server.getResponse().value.toString(),
//              String(server.getResponse().output),
              server.getResponse());
    }

    function callFailure(event : ErrorEvent) : void
    {
      cleanup();
      failure(event, server.getFault());
    }

    public function cleanup() : void
    {
      server.removeEventListener(Event.COMPLETE, callSuccess);
      server.removeEventListener(ErrorEvent.ERROR, callFailure);
    }

    var module : String;
    var method : String;
    var url : String;
    var param : Object;
    var server : ConnectionImpl;
    var success : Function;
    var failure : Function;

    static var XMLRPC_SERVER : String = "boss.emulab.net";
    static var SERVER_PATH : String = ":443/protogeni/xmlrpc/";
  }
}
