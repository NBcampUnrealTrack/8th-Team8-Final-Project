StrikeForge 미니맵 QA 증빙 (2026-10-19)

공식 집계: final 폴더의 자동 기능 테스트 8항목 x 3회 = 24회 통과.
세부 판정: 132/132. 단위 테스트는 unit-final의 별도 1건 Success.

보고서: StrikeForge_Minimap_QA_2026-10-19.pdf
요약 결과표: checklist_results.csv
실측 좌표 및 세부 판정: final/assertions.csv
화면 증거: final/*.png
실행 환경: final/environment.txt
원본 로그: rendered-final.log / unit-final.log
테스트 소스: SFMinimapRenderedQA.cpp
과거 수정 근거: b857ed46.diff
빌드와 관련 소스 해시: tested-build-hashes.json

attempt01, attempt02는 합성 입력 하니스의 진단 실행이며 공식 통과율 집계와 게임 결함 건수에 포함하지 않았습니다.
전체 게임 수동 테스트, OS Alt+Tab, 별도 제작된 경계 미설정 맵, 패키징 빌드 검증을 완료한 것으로 표현하지 않았습니다.
