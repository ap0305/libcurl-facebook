# libcurl-facebook

This is test code for uploading image and getting result from face detection web service using C++ libcurl library.

- Command
curl -v -H "Content-Type: application/octet-stream" -H "Content-length: 49640" -H "Ocp-Apim-Subscription-Key: 420ae629d4a645a5841d45526219ace1" --data-binary "@3.png" "https://westus.api.cognitive.microsoft.com/face/v1.0/detect?returnFaceId=true&returnFaceLandmarks=false&returnFaceAttributes=age,gender"

- Implementation by C++
cd src
make
./libcurl_fp_upload [ image file path ]
