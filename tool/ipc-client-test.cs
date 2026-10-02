using System.IO.Pipes;
using System.Text;

using var client = new NamedPipeClientStream(".", "LuaSTGFlux", PipeDirection.InOut);
client.Connect(2000);
client.ReadMode = PipeTransmissionMode.Message;

var request = """{"id":1,"function":"quit","args":[1]}""";
var bytes = Encoding.UTF8.GetBytes(request);
client.Write(bytes, 0, bytes.Length);
client.Flush();

var buffer = new byte[65536];
int read = client.Read(buffer, 0, buffer.Length);
Console.WriteLine(Encoding.UTF8.GetString(buffer, 0, read));
