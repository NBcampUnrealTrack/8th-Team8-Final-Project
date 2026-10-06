import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const ROOT='C:/FinalPro/Finalproject';
const DIR=ROOT+'/Portfolio';
const SKILL='C:/Users/cch77/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations';
const NM='C:/Users/cch77/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules';
process.env.RUNTIME_NODE_MODULES=NM;
const {Presentation,PresentationFile,FileBlob}=await import(pathToFileURL(NM+'/@oai/artifact-tool/dist/artifact_tool.mjs'));
const {finalizePresentation}=await import(pathToFileURL(SKILL+'/container_tools/artifact_tool_utils.mjs'));
const p=Presentation.create({slideSize:{width:1280,height:720}});
const C={bg:'#0B0B0B',blue:'#3984FF',white:'#FFFFFF'};
const font='Malgun Gothic';
function text(s,t,x,y,w,h,size=26,color=C.white,bold=false,align='left'){
 const a=s.shapes.add({geometry:'textbox',name:t.slice(0,32),position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
 a.text=t;a.text.style={typeface:font,fontSize:size,bold,color,autoFit:'none',alignment:align,verticalAlignment:'middle',insets:{left:0,right:0,top:0,bottom:0}};return a;
}
function rect(s,x,y,w,h,fill='none',stroke=C.blue,width=1){return s.shapes.add({geometry:'rect',position:{left:x,top:y,width:w,height:h},fill,line:{fill:stroke,width}});}
function base(n,title,sub){const s=p.slides.add();s.background.fill=C.bg;text(s,String(n).padStart(2,'0'),1170,42,54,28,20,C.blue,true,'right');text(s,title,64,52,1080,65,44,C.white,true);if(sub)text(s,sub,64,125,1120,36,23);text(s,'정창희  /  StrikeForge',64,672,920,22,16,C.white);return s;}
function note(s,t){s.speakerNotes.textFrame.setText(t);}
const src=(paths)=>paths.map(x=>ROOT+'/Source/StrikeForge/'+x).join('\n');
const s1=p.slides.add();s1.background.fill=C.bg;
s1.images.add({blob:new Uint8Array(await fs.readFile('C:/Users/cch77/OneDrive/Desktop/증명사진.jpg')),contentType:'image/jpeg',alt:'정창희 증명사진',fit:'contain',position:{left:72,top:105,width:365,height:470}});
text(s1,'GAME PROGRAMMER',508,101,650,40,24,C.blue,true);
text(s1,'정창희',502,165,680,115,76,C.white,true);
text(s1,'유닛 AI · 튜토리얼 · UI · 스토리',508,291,700,50,29);
text(s1,'010-2371-7654',508,420,680,47,34);
text(s1,'cch77654@gmail.com',508,483,700,47,32);
text(s1,'PORTFOLIO',72,644,370,32,21,C.blue,true);
text(s1,'StrikeForge  /  Unreal Engine 5.5',508,638,690,38,23);
note(s1,'사용자 제공 이름, 연락처, 세 번째 사진을 사용. 담당 범위: 유닛 AI 관련 기능, 튜토리얼 기능(UI와 스토리 포함). 프로젝트 식별명은 현재 저장소의 StrikeForge.uproject 기준. 팀 인원과 기간은 미확인.');
const s2=base(2,'목차','StrikeForge   /   Unreal Engine 5.5   /   유닛 AI와 튜토리얼 담당');
const toc=[['01','유닛 AI 전투 판단','전투 상태 전환과 타깃 선정','03'],['02','튜토리얼 진행 제어','목표 달성 이벤트와 진행 상태 분리','04'],['03','스토리 분기·연출','대화 데이터와 선택지 연결','05']];
toc.forEach((a,i)=>{const y=221+i*127;text(s2,a[0],64,y,82,62,43,C.blue,true);text(s2,a[1],174,y-3,850,52,34,C.white,true);text(s2,a[2],174,y+52,920,33,23);text(s2,a[3],1128,y+8,84,44,28,C.blue,false,'right');});
text(s2,'세부 구현 후보   06–11',174,615,880,30,21,C.blue);
note(s2,'대표 콘텐츠 3개는 사용자 담당 영역을 바탕으로 추천한 구성. 최종 강조 순위는 본인 설명 가능 범위와 튜터 피드백에 맞춰 조정. 전체 프로젝트를 단독 제작했다고 주장하지 않는다.');
const features=[
 {title:'유닛 AI 전투 판단',sub:'전진 · 추격 · 교전 · 사망 상태와 타깃 선정',visual:'전투 장면 + 상태 전환도',detail:'현재 상태와 전환 조건이 보이는 삽화',focus:'전투 판단과 이동 책임 분리',proof:'상태 전환이 보이는 짧은 시연',files:['Units/Unit/UnitBase.h','Units/Unit/UnitBase.cpp','Units/Unit/UnitAIController.h','Game/Subsystem/SFCombatSubsystem.h'],extra:'전투 FSM은 UnitBase가, 이동 요청은 UnitAIController가 담당. 타깃 탐색과 사거리 판정의 구체적 우선순위는 본인 설명을 추가. Behavior Tree 또는 EQS 사용으로 표기하지 않는다.'},
 {title:'튜토리얼 진행 제어',sub:'목표 달성을 기다리고 다음 안내를 이어가는 구조',visual:'안내 / 플레이 / 목표 달성',detail:'위젯이 닫힌 동안에도 이어지는 진행 흐름',focus:'진행 상태와 UI 수명 분리',proof:'안내 종료 후 재개되는 장면',files:['InGame/HUD/Tutorial/SFTutorialSubsystem.h','InGame/HUD/Tutorial/SFTutorialSubsystem.cpp','InGame/HUD/Tutorial/SFStageIntroData.h'],extra:'WorldSubsystem이 대기 상태를 보관하며 GameplayTag 목표 신호를 받는다. 이미 완료한 목표를 대기하지 않는 처리와 재진입 방지를 설명 후보로 활용.'},
 {title:'스토리 분기·연출',sub:'대화 노드와 선택지로 구성한 스테이지 스토리',visual:'대화 화면 + 선택지 분기',detail:'선택 전후의 흐름이 보이는 화면 두 장',focus:'대화 내용과 진행 규칙 분리',proof:'선택지와 다음 노드 연결 장면',files:['InGame/HUD/Tutorial/SFStageIntroData.h','InGame/HUD/Tutorial/SFStageIntroWidget.cpp'],extra:'DataTable 행에 대화, 선택지, NextNodeIndex, 초상화, 가이드 데이터를 보관. 분기는 대화 노드 분기를 의미하며 멀티 엔딩이나 저장 기능으로 확대해서 말하지 않는다.'},
 {title:'군집 이동과 경로 재요청',sub:'NavMesh와 DetourCrowd를 사용하는 이동 제어',visual:'같은 구간의 이동 비교',detail:'다수 유닛이 목표에 접근하는 장면',focus:'이동 요청 빈도와 반응성 조절',proof:'재요청 로그와 이동 시연',files:['Units/Unit/UnitAIController.h','Units/Unit/UnitAIController.cpp'],extra:'같은 목표에 대한 중복 요청 억제와 슬롯 추적 전용 재요청 간격이 있다. 기본 값 0.5초와 0.2초는 설정값이며 성능 개선 측정치가 아니다. 수정 전 영상이 없다면 전후 비교 대신 현재 동작 시연을 사용.'},
 {title:'근접 공격 위치 분산',sub:'대상 주변의 공격 슬롯을 배정하고 반환하는 구조',visual:'타깃 주변 공격 위치',detail:'여러 유닛의 접근 위치를 표시한 삽화',focus:'공격 슬롯의 점유와 반환',proof:'진입·사망·타깃 변경 시연',files:['Game/Subsystem/SFCombatSubsystem.h','Game/Subsystem/SFCombatSubsystem.cpp','Units/Unit/UnitBase.cpp'],extra:'ClaimAttackSlot / ReleaseAttackSlot / GetAttackSlotLocation 근거. 공격 슬롯 주변 분산이 모든 군집 정체를 해결한다는 표현은 피하고 재현한 조건을 제시.'},
 {title:'플레이어 명령과 자동 전투',sub:'이동 · 공격 이동 · 대상 공격 · 정지 · 제자리 고수',visual:'같은 상황, 다른 명령',detail:'이동과 공격 이동의 행동 차이를 보여 주는 장면',focus:'명령에 따른 전투 규칙 구분',proof:'명령 교체와 예외 상황 시연',files:['Units/Unit/UnitBase.h','Units/Unit/UnitBase.cpp','Tests/SFForcedAttackTests.cpp'],extra:'ExecuteCommand와 명령 큐 정의가 존재. 일반 이동은 반격하지 않고, 공격 이동은 도중 교전하며, HoldGround는 사거리 내 공격만 한다. 예외 처리는 실제 구현과 시험 결과를 확인해 적는다.'},
 {title:'튜토리얼 UI와 스포트라이트',sub:'현재 조작할 화면 영역과 월드 위치를 강조하는 안내',visual:'가이드 UI 대표 화면',detail:'스포트라이트와 설명 영역을 크게 보여 주는 캡처',focus:'조작 대상과 완료 조건 연결',proof:'해상도별 안내 위치와 입력 시연',files:['InGame/HUD/Tutorial/SFTutorialSpotlightWidget.h','InGame/HUD/Tutorial/SFTutorialSpotlightWidget.cpp','InGame/HUD/Tutorial/SFTutorialGhostActor.cpp','InGame/HUD/Tutorial/SFStageIntroData.h'],extra:'화면 영역 / 월드 대상, 클릭 / 목표 태그 완료 조건, 고스트 가이드 정의가 있다. 실제 지원 해상도나 사용성 개선률은 별도 확인.'},
 {title:'튜토리얼과 적 스폰 연동',sub:'목표 달성 시점에 맞춰 적의 등장을 제어하는 구조',visual:'목표 완료 전 / 완료 후',detail:'같은 맵에서 적 등장 시점이 달라지는 장면',focus:'시나리오 게이트와 스폰 토글 분리',proof:'선행 완료·재활성화 상황 검증',files:['Units/Spawner/UnitSpawner.h','Units/Spawner/UnitSpawner.cpp','InGame/HUD/Tutorial/SFTutorialSubsystem.h','Tests/SFTutorialSpawnGateTests.cpp'],extra:'OnObjectiveRecorded 신호와 별도 활성화 상태를 사용. 튜토리얼이 대기하지 않을 때도 완료 기록을 알리는 이유, bActivated와 bSpawningEnabled를 분리한 이유가 설명 후보.'},
 {title:'스토리 다시 보기',sub:'챕터 해금과 페이지 전환을 갖춘 로비 스토리 UI',visual:'스토리 UI 대표 화면',detail:'챕터 전환 또는 해금 전후를 보여 주는 캡처',focus:'진행도와 열람 가능한 챕터 연결',proof:'닫기·재열기·챕터 전환 시연',files:['UI/SFStoryWidget.h','UI/SFStoryWidget.cpp','UI/SFStoryIllustrationActor.cpp','Game/GameInstance/SFGameInstance.h'],extra:'OpenStory / CloseStory / TurnPage, AvailableChapterCount, 일러스트 액터 수명 관리가 있다. 재실행 간 영구 저장 여부는 별도 확인.'}
];
features.forEach((f,i)=>{
 const s=base(i+3,f.title,f.sub);
 rect(s,64,207,748,421,'none',C.blue,1.2);
 text(s,'이미지 교체 영역',88,229,700,28,19,C.blue);
 text(s,f.visual,96,358,684,53,33,C.white,true,'center');
 text(s,f.detail,96,419,684,43,21,C.white,false,'center');
 text(s,'16:9 캡처 또는 핵심 구조 삽화',96,568,684,28,18,C.blue,false,'center');
 const x=866,w=348;
 text(s,'핵심 설계',x,211,w,30,22,C.blue,true);
 text(s,f.focus,x,253,w,80,28,C.white,true);
 text(s,'해결한 문제',x,362,w,29,22,C.blue,true);
 text(s,'[문제 1문장]\n[해결 방법 1문장]',x,402,w,76,24);
 text(s,'보여 줄 근거',x,521,w,29,22,C.blue,true);
 text(s,f.proof,x,560,w,68,23);
 note(s,`${f.extra}\n\n사용자 담당 범위와 연결된 후보 슬라이드. 세부 기능별 직접 구현·수정 범위와 정량 성과는 작성자가 확인 후 추가. 이미지 교체 영역과 대괄호 문구는 의도된 템플릿 자리표시자.\n\n코드 근거:\n${src(f.files)}`);
});
const last=base(12,'정창희','유닛 AI와 튜토리얼을 구현하는 게임 개발자');
text(last,'010-2371-7654',64,277,1060,64,46,C.white,true);
text(last,'cch77654@gmail.com',64,368,1120,64,42);
text(last,'GitHub / 시연 영상',64,526,1000,32,23,C.blue,true);
text(last,'[프로젝트 링크 입력]',64,574,1050,45,27);
note(last,'사용자 제공 연락처. GitHub 및 시연 영상 주소는 제공되지 않아 입력 자리만 배치.');
await fs.writeFile(DIR+'/.build/features.json',JSON.stringify(features,null,2));
await (await PresentationFile.exportPptx(p)).save(DIR+'/.build/candidate.pptx');
await finalizePresentation({workspaceDir:DIR,candidatePath:DIR+'/.build/candidate.pptx',finalPath:DIR+'/output/정창희_포트폴리오_뼈대.pptx',pythonExecutable:'C:/Users/cch77/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',integrityValidatorPath:SKILL+'/container_tools/inspect_presentation_package_integrity.py',layoutValidatorPath:SKILL+'/container_tools/inspect_presentation_layout_geometry.py',layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit'],explicitTotalSlideCount:12,requiredNativeTableOwnerSlides:[],fontPolicy:{basis:'design',families:[font]},verifyArtifactToolImport:true,receiptPath:DIR+'/.build/validation.json'});
const finalDeck=await PresentationFile.importPptx(await FileBlob.load(DIR+'/output/정창희_포트폴리오_뼈대.pptx'));
for(let i=0;i<finalDeck.slides.items.length;i++){
 const slide=finalDeck.slides.items[i];
 const img=await finalDeck.export({slide,format:'png',scale:1});
 await fs.writeFile(DIR+`/.build/slide-${String(i+1).padStart(2,'0')}.png`,new Uint8Array(await img.arrayBuffer()));
 console.log('rendered',i+1);
}
console.log('done');

