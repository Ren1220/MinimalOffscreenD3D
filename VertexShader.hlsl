// taki shader nie potrzebuje danych wej�ciowych, dzia�a na podstawie kolejnych ID wierzcho�k�w
// wymaga za to Shader Model 4.0 - zmiana ustawie� domy�lnych
float4 main( uint id : SV_VertexID, out float2 coords : TEXCOORD ) : SV_POSITION
{
	coords = float2( // wsp�rz�dne tekstury zamieni� si� w docelowy kolor
		id & 1 ? 0 : 1,  // x: 0 | 1 | 0 | 1
		id & 2 ? 1 : 0); // y: 1 | 1 | 0 | 0
	return float4(coords, 0, 1);
}